/**
 ******************************************************************************
 * @file    bmp280.c
 * @brief   Implementação do driver BMP280 (Bosch Sensortec)
 *
 * Referência: BMP280 Datasheet — BST-BMP280-DS001-19 (Rev. 1.19)
 *   Seção 4.2.3 — Compensation formulas in double precision floating point
 *   (usamos a versão inteira de 64 bits por ser mais portável em bare-metal)
 ******************************************************************************
 */

#include "bmp280.h"
#include <math.h>

/* ─── Mapa de registradores internos ─────────────────────────────────────── */
#define REG_CHIP_ID      0xD0u  /**< Chip ID (deve retornar 0x60)           */
#define REG_RESET        0xE0u  /**< Soft-reset: escrever 0xB6              */
#define REG_STATUS       0xF3u  /**< bit3=measuring, bit0=im_update         */
#define REG_CTRL_MEAS    0xF4u  /**< osrs_t[7:5] | osrs_p[4:2] | mode[1:0] */
#define REG_CONFIG       0xF5u  /**< t_sb[7:5] | filter[4:2] | spi3w_en[0] */
#define REG_PRESS_MSB    0xF7u  /**< Início do bloco de dados (6 bytes)     */
#define REG_CALIB_START  0x88u  /**< Início do bloco de calibração (24 b.)  */

#define RESET_VALUE      0xB6u
#define STATUS_MEASURING (1u << 3)

/* ─── Helpers de I²C (leitura/escrita de registrador) ───────────────────── */

static BMP280_Status_t _write_reg(BMP280_Handle_t *hdev,
                                  uint8_t          reg,
                                  uint8_t          value)
{
    uint8_t buf[2] = { reg, value };
    HAL_StatusTypeDef ret = HAL_I2C_Master_Transmit(
        hdev->hi2c, hdev->dev_addr, buf, 2, BMP280_I2C_TIMEOUT);
    return (ret == HAL_OK) ? BMP280_OK : BMP280_ERR_I2C;
}

static BMP280_Status_t _read_regs(BMP280_Handle_t *hdev,
                                  uint8_t          reg,
                                  uint8_t         *dst,
                                  uint16_t         len)
{
    /* Aponta para o registrador inicial */
    HAL_StatusTypeDef ret = HAL_I2C_Master_Transmit(
        hdev->hi2c, hdev->dev_addr, &reg, 1, BMP280_I2C_TIMEOUT);
    if (ret != HAL_OK) return BMP280_ERR_I2C;

    /* Lê 'len' bytes consecutivos */
    ret = HAL_I2C_Master_Receive(
        hdev->hi2c, hdev->dev_addr, dst, len, BMP280_I2C_TIMEOUT);
    return (ret == HAL_OK) ? BMP280_OK : BMP280_ERR_I2C;
}

/* ─── Leitura dos coeficientes de calibração da NVM ─────────────────────── */

static BMP280_Status_t _read_calibration(BMP280_Handle_t *hdev)
{
    uint8_t cal[24];
    BMP280_Status_t st = _read_regs(hdev, REG_CALIB_START, cal, sizeof(cal));
    if (st != BMP280_OK) return st;

    /*
     * O BMP280 armazena os coeficientes em little-endian (LSB no endereço
     * menor). A macro abaixo reconstrói cada word de 16 bits corretamente.
     */
#define CALIB_U16(idx) ((uint16_t)((cal[(idx)+1] << 8) | cal[(idx)]))
#define CALIB_S16(idx) ((int16_t) ((cal[(idx)+1] << 8) | cal[(idx)]))

    hdev->dig_T1 = CALIB_U16(0);
    hdev->dig_T2 = CALIB_S16(2);
    hdev->dig_T3 = CALIB_S16(4);

    hdev->dig_P1 = CALIB_U16(6);
    hdev->dig_P2 = CALIB_S16(8);
    hdev->dig_P3 = CALIB_S16(10);
    hdev->dig_P4 = CALIB_S16(12);
    hdev->dig_P5 = CALIB_S16(14);
    hdev->dig_P6 = CALIB_S16(16);
    hdev->dig_P7 = CALIB_S16(18);
    hdev->dig_P8 = CALIB_S16(20);
    hdev->dig_P9 = CALIB_S16(22);

#undef CALIB_U16
#undef CALIB_S16

    return BMP280_OK;
}

/* ─── Fórmulas de compensação (inteira de 64 bits — datasheet Bosch §4.2.3) */

/**
 * @brief  Compensa o valor ADC bruto de temperatura.
 *         Efeito colateral: atualiza hdev->t_fine, necessário para
 *         compensar a pressão em seguida.
 *
 * @param  hdev      Handle do sensor (calibração já carregada).
 * @param  adc_T     Valor ADC de 20 bits (raw, já deslocado).
 * @return Temperatura em °C (float).
 */
static float _compensate_temperature(BMP280_Handle_t *hdev, int32_t adc_T)
{
    int32_t var1, var2;

    /*
     * Fórmula do datasheet (versão inteira de 32 bits):
     *
     *   var1 = ((adc_T >> 3) - (dig_T1 << 1)) * dig_T2 >> 11
     *   var2 = (((adc_T >> 4) - dig_T1)^2 >> 12) * dig_T3 >> 14
     *   t_fine = var1 + var2
     *   T = (t_fine * 5 + 128) >> 8          [resultado em centésimos de °C]
     */
    var1 = ((adc_T >> 3) - ((int32_t)hdev->dig_T1 << 1));
    var1 = (var1 * (int32_t)hdev->dig_T2) >> 11;

    var2 = (adc_T >> 4) - (int32_t)hdev->dig_T1;
    var2 = (((var2 * var2) >> 12) * (int32_t)hdev->dig_T3) >> 14;

    hdev->t_fine = var1 + var2;

    return (float)((hdev->t_fine * 5 + 128) >> 8) / 100.0f;
}

/**
 * @brief  Compensa o valor ADC bruto de pressão.
 *         DEVE ser chamada após _compensate_temperature (depende de t_fine).
 *
 * @param  hdev      Handle do sensor.
 * @param  adc_P     Valor ADC de 20 bits (raw, já deslocado).
 * @return Pressão em hPa (float).  Retorna 0.0f em caso de overflow.
 */
static float _compensate_pressure(BMP280_Handle_t *hdev, int32_t adc_P)
{
    /*
     * Fórmula do datasheet (versão inteira de 64 bits):
     * Necessária para não perder precisão nas multiplicações intermediárias.
     */
    int64_t var1, var2, p;

    var1 = (int64_t)hdev->t_fine - 128000LL;
    var2 = var1 * var1 * (int64_t)hdev->dig_P6;
    var2 = var2 + ((var1 * (int64_t)hdev->dig_P5) << 17);
    var2 = var2 + (((int64_t)hdev->dig_P4) << 35);
    var1 = ((var1 * var1 * (int64_t)hdev->dig_P3) >> 8)
         + ((var1 * (int64_t)hdev->dig_P2) << 12);
    var1 = (((int64_t)1 << 47) + var1) * (int64_t)hdev->dig_P1 >> 33;

    if (var1 == 0) return 0.0f; /* Evita divisão por zero */

    p    = 1048576LL - (int64_t)adc_P;
    p    = (((p << 31) - var2) * 3125LL) / var1;
    var1 = ((int64_t)hdev->dig_P9 * (p >> 13) * (p >> 13)) >> 25;
    var2 = ((int64_t)hdev->dig_P8 * p) >> 19;
    p    = ((p + var1 + var2) >> 8) + ((int64_t)hdev->dig_P7 << 4);

    /* p está em Pa com 8 bits fracionários → divide por 256 para Pa inteiro
     * e depois por 100 para hPa */
    return (float)(uint32_t)(p >> 8) / 100.0f;
}

/**
 * @brief  Calcula altitude relativa pela fórmula barométrica internacional.
 *
 * @param  pressure_hpa    Pressão medida em hPa.
 * @param  sea_level_hpa   Pressão ao nível do mar em hPa.
 * @return Altitude estimada em metros.
 */
static float _calc_altitude(float pressure_hpa, float sea_level_hpa)
{
    /*
     * Fórmula barométrica (ISA):
     *   h = 44330 * (1 - (P / P0)^(1/5.255))
     *
     * Válida até ~11 km. Para uso acadêmico em campo aberto é suficiente.
     * Nota: o resultado é sensível à pressão de referência (P0).
     *       Para maior precisão, use a pressão atual da estação METAR mais
     *       próxima (disponível via ASOS/INMET).
     */
    return 44330.0f * (1.0f - powf(pressure_hpa / sea_level_hpa, 0.190295f));
}

/* ─── API pública ────────────────────────────────────────────────────────── */

BMP280_Status_t BMP280_Init(BMP280_Handle_t       *hdev,
                            I2C_HandleTypeDef     *hi2c,
                            uint8_t                addr_7bit,
                            const BMP280_Config_t *cfg)
{
    /* Armazena recursos de hardware */
    hdev->hi2c         = hi2c;
    hdev->dev_addr     = (uint16_t)(addr_7bit << 1); /* HAL usa endereço 8 bits */
    hdev->sea_level_hpa = cfg->sea_level_hpa;
    hdev->t_fine       = 0;
    hdev->temperature  = 0.0f;
    hdev->pressure     = 0.0f;
    hdev->altitude     = 0.0f;

    BMP280_Status_t st;

    /* ── 1. Verifica presença e identidade do chip ───────────────────── */
    uint8_t chip_id = 0;
    st = _read_regs(hdev, REG_CHIP_ID, &chip_id, 1);
    if (st != BMP280_OK)              return BMP280_ERR_I2C;
    if (chip_id != BMP280_CHIP_ID)    return BMP280_ERR_NOT_FOUND;

    /* ── 2. Soft-reset para estado limpo ─────────────────────────────── */
    st = _write_reg(hdev, REG_RESET, RESET_VALUE);
    if (st != BMP280_OK) return st;
    HAL_Delay(10); /* Aguarda reinicialização do sensor (~2 ms conforme datasheet) */

    /* ── 3. Lê coeficientes de calibração da NVM ─────────────────────── */
    st = _read_calibration(hdev);
    if (st != BMP280_OK) return st;

    /* ── 4. Registrador config: standby + filtro IIR ─────────────────── */
    /*
     * Bit layout de REG_CONFIG [7:0]:
     *   [7:5] t_sb   — tempo de standby (modo Normal)
     *   [4:2] filter — coeficiente IIR
     *   [0]   spi3w_en — 0 = I²C (não alterar)
     */
    uint8_t reg_config = (uint8_t)((cfg->standby << 5) | (cfg->filter << 2));
    st = _write_reg(hdev, REG_CONFIG, reg_config);
    if (st != BMP280_OK) return st;

    /* ── 5. Registrador ctrl_meas: oversampling + modo ───────────────── */
    /*
     * Bit layout de REG_CTRL_MEAS [7:0]:
     *   [7:5] osrs_t — oversampling temperatura
     *   [4:2] osrs_p — oversampling pressão
     *   [1:0] mode   — Sleep / Forced / Normal
     */
    uint8_t reg_ctrl = (uint8_t)((cfg->os_temperature << 5) |
                                 (cfg->os_pressure    << 2) |
                                  cfg->mode);
    st = _write_reg(hdev, REG_CTRL_MEAS, reg_ctrl);
    if (st != BMP280_OK) return st;

    /* Aguarda a primeira medição completar antes de retornar.
         * t_meas(max) com osrs_T=x2 + osrs_P=x16 ≈ 191 ms (datasheet §4.1).
         * 250 ms garante margem de segurança independente da config. */
        HAL_Delay(250);

    return BMP280_OK;
}

BMP280_Status_t BMP280_Read(BMP280_Handle_t *hdev)
{
    BMP280_Status_t st;

    /* ── Verifica se o sensor ainda está medindo (modo Forced) ───────── */
    uint8_t status = 0;
    st = _read_regs(hdev, REG_STATUS, &status, 1);
    if (st != BMP280_OK) return st;
    if (status & STATUS_MEASURING) return BMP280_ERR_BUSY;

    /* ── Lê os 6 bytes de dados brutos (pressão + temperatura) ──────── */
    /*
     * Mapa de memória a partir de 0xF7:
     *   [0] press_msb  [1] press_lsb  [2] press_xlsb
     *   [3] temp_msb   [4] temp_lsb   [5] temp_xlsb
     *
     * Cada valor é de 20 bits: MSB[7:0] | LSB[7:0] | XLSB[7:4]
     * O resultado bruto deve ser deslocado 4 bits para a direita.
     */
    uint8_t raw[6];
    st = _read_regs(hdev, REG_PRESS_MSB, raw, 6);
    if (st != BMP280_OK) return st;

    int32_t adc_P = (int32_t)(((uint32_t)raw[0] << 12) |
                               ((uint32_t)raw[1] <<  4) |
                               ((uint32_t)raw[2] >>  4));

    int32_t adc_T = (int32_t)(((uint32_t)raw[3] << 12) |
                               ((uint32_t)raw[4] <<  4) |
                               ((uint32_t)raw[5] >>  4));

    /* ── Aplica compensação (temperatura primeiro — atualiza t_fine) ── */
    hdev->temperature = _compensate_temperature(hdev, adc_T);
    hdev->pressure    = _compensate_pressure(hdev, adc_P);
    hdev->altitude    = _calc_altitude(hdev->pressure, hdev->sea_level_hpa);

    return BMP280_OK;
}

BMP280_Status_t BMP280_Reset(BMP280_Handle_t *hdev)
{
    return _write_reg(hdev, REG_RESET, RESET_VALUE);
}

BMP280_Status_t BMP280_IsConnected(BMP280_Handle_t *hdev)
{
    uint8_t chip_id = 0;
    BMP280_Status_t st = _read_regs(hdev, REG_CHIP_ID, &chip_id, 1);
    if (st != BMP280_OK || chip_id != BMP280_CHIP_ID) return BMP280_ERR_NOT_FOUND;
    return BMP280_OK;
}

const char *BMP280_StatusStr(BMP280_Status_t status)
{
    switch (status) {
        case BMP280_OK:            return "OK";
        case BMP280_ERR_NOT_FOUND: return "SENSOR NAO ENCONTRADO - Verifique endereco e fiacao I2C";
        case BMP280_ERR_I2C:       return "FALHA I2C - Verifique SDA/SCL e resistores de pull-up";
        case BMP280_ERR_BUSY:      return "SENSOR OCUPADO - Aguardando fim da medicao";
        default:                   return "ERRO DESCONHECIDO";
    }
}
