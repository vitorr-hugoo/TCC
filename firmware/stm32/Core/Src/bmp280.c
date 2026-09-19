/**
 ******************************************************************************
 * @file    bmp280.c
 * @brief   Implementação do driver BMP280 (Bosch Sensortec)
 *
 * Referência: BMP280 Datasheet — BST-BMP280-DS001-19 (Rev. 1.19)
 *   Seção 4.2.3 — Compensation formulas in double precision floating point
 *   (calculo em double, com validacao antes de atualizar a amostra)
 ******************************************************************************
 */

#include "bmp280.h"
#include <math.h>

/* ─── Mapa de registradores internos ─────────────────────────────────────── */
#define REG_CHIP_ID     0xD0u /**< Chip ID (deve retornar 0x58)           */
#define REG_RESET       0xE0u /**< Soft-reset: escrever 0xB6              */
#define REG_STATUS      0xF3u /**< bit3=measuring, bit0=im_update         */
#define REG_CTRL_MEAS   0xF4u /**< osrs_t[7:5] | osrs_p[4:2] | mode[1:0] */
#define REG_CONFIG      0xF5u /**< t_sb[7:5] | filter[4:2] | spi3w_en[0] */
#define REG_PRESS_MSB   0xF7u /**< Início do bloco de dados (6 bytes)     */
#define REG_CALIB_START 0x88u /**< Início do bloco de calibração (24 b.)  */

#define RESET_VALUE      0xB6u
#define STATUS_MEASURING (1u << 3)

/* ─── Helpers de I²C (leitura/escrita de registrador) ───────────────────── */

static BMP280_Status_t _write_reg(BMP280_Handle_t *hdev, uint8_t reg, uint8_t value)
{
    if (!hdev || !hdev->hi2c)
    {
        return BMP280_ERR_CONFIG;
    }
    uint8_t buf[2] = {reg, value};
    HAL_StatusTypeDef ret =
        HAL_I2C_Master_Transmit(hdev->hi2c, hdev->dev_addr, buf, 2, BMP280_I2C_TIMEOUT);
    return (ret == HAL_OK) ? BMP280_OK : BMP280_ERR_I2C;
}

static BMP280_Status_t _read_regs(BMP280_Handle_t *hdev, uint8_t reg, uint8_t *dst, uint16_t len)
{
    if (!hdev || !hdev->hi2c)
    {
        return BMP280_ERR_CONFIG;
    }
    /* Aponta para o registrador inicial */
    HAL_StatusTypeDef ret =
        HAL_I2C_Master_Transmit(hdev->hi2c, hdev->dev_addr, &reg, 1, BMP280_I2C_TIMEOUT);
    if (ret != HAL_OK)
    {
        return BMP280_ERR_I2C;
    }

    /* Lê 'len' bytes consecutivos */
    ret = HAL_I2C_Master_Receive(hdev->hi2c, hdev->dev_addr, dst, len, BMP280_I2C_TIMEOUT);
    return (ret == HAL_OK) ? BMP280_OK : BMP280_ERR_I2C;
}

/* ─── Leitura dos coeficientes de calibração da NVM ─────────────────────── */

static BMP280_Status_t _read_calibration(BMP280_Handle_t *hdev)
{
    uint8_t cal[24];
    BMP280_Status_t st = _read_regs(hdev, REG_CALIB_START, cal, sizeof(cal));
    if (st != BMP280_OK)
    {
        return st;
    }

    /*
     * O BMP280 armazena os coeficientes em little-endian (LSB no endereço
     * menor). A macro abaixo reconstrói cada word de 16 bits corretamente.
     */
#define CALIB_U16(idx) ((uint16_t)((cal[(idx) + 1] << 8) | cal[(idx)]))
#define CALIB_S16(idx) ((int16_t)((cal[(idx) + 1] << 8) | cal[(idx)]))

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

    if (hdev->dig_P1 == 0 || hdev->dig_P1 == UINT16_MAX || hdev->dig_T1 == 0 ||
        hdev->dig_T1 == UINT16_MAX)
    {
        return BMP280_ERR_CALIBRATION;
    }
    return BMP280_OK;
}

/* ─── Formulas de compensacao (double — datasheet Bosch §4.2.3) */

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
    /* Bosch double-precision formula avoids signed-shift/overflow UB if
     * a coefficient is corrupted. Candidate outputs are validated below. */
    double v1 = ((double)adc_T / 16384.0 - hdev->dig_T1 / 1024.0) * hdev->dig_T2;
    double d = (double)adc_T / 131072.0 - hdev->dig_T1 / 8192.0;
    double v2 = d * d * hdev->dig_T3;
    hdev->t_fine = (int32_t)(v1 + v2);
    return (float)((v1 + v2) / 5120.0);
}

/**
 * @brief  Compensa o valor ADC bruto de pressão.
 *         DEVE ser chamada após _compensate_temperature (depende de t_fine).
 *
 * @param  hdev      Handle do sensor.
 * @param  adc_P     Valor ADC de 20 bits (raw, já deslocado).
 * @return Pressão em hPa (float).  Retorna NAN se o divisor for zero.
 */
static float _compensate_pressure(BMP280_Handle_t *hdev, int32_t adc_P)
{
    double v1 = hdev->t_fine / 2.0 - 64000.0;
    double v2 = v1 * v1 * hdev->dig_P6 / 32768.0;
    v2 += v1 * hdev->dig_P5 * 2.0;
    v2 = v2 / 4.0 + hdev->dig_P4 * 65536.0;
    v1 = (hdev->dig_P3 * v1 * v1 / 524288.0 + hdev->dig_P2 * v1) / 524288.0;
    v1 = (1.0 + v1 / 32768.0) * hdev->dig_P1;
    if (v1 == 0)
    {
        return NAN;
    }
    double p = (1048576.0 - adc_P - v2 / 4096.0) * 6250.0 / v1;
    v1 = hdev->dig_P9 * p * p / 2147483648.0;
    v2 = p * hdev->dig_P8 / 32768.0;
    return (float)((p + (v1 + v2 + hdev->dig_P7) / 16.0) / 100.0);
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

/* Polling is bounded even if HAL_GetTick stops; I2C has its own timeout. */
static BMP280_Status_t _wait_ready(BMP280_Handle_t *hdev)
{
    for (uint32_t waited = 0; waited <= hdev->measurement_timeout_ms; ++waited)
    {
        uint8_t status;
        BMP280_Status_t st = _read_regs(hdev, REG_STATUS, &status, 1);
        if (st != BMP280_OK)
        {
            return st;
        }
        if (!(status & (STATUS_MEASURING | 1u)))
        {
            return BMP280_OK;
        }
        if (waited < hdev->measurement_timeout_ms)
        {
            HAL_Delay(1);
        }
    }
    return BMP280_ERR_BUSY;
}
static BMP280_Status_t _check_config(BMP280_Handle_t *hdev)
{
    BMP280_Status_t st = BMP280_IsConnected(hdev);
    if (st != BMP280_OK)
    {
        return st;
    }
    uint8_t regs[2];
    st = _read_regs(hdev, REG_CTRL_MEAS, regs, 2);
    if (st != BMP280_OK)
    {
        return st;
    }
    uint8_t mask = (hdev->expected_ctrl & 3u) == BMP280_MODE_NORMAL ? 0xff : 0xfc;
    if ((regs[0] & mask) != (hdev->expected_ctrl & mask) ||
        (regs[1] & 0xfdu) != hdev->expected_config)
    {
        hdev->initialized = 0;
        return BMP280_ERR_CONFIG;
    }
    return BMP280_OK;
}

/* ─── API pública ────────────────────────────────────────────────────────── */

BMP280_Status_t BMP280_Init(BMP280_Handle_t *hdev, I2C_HandleTypeDef *hi2c, uint8_t addr_7bit,
                            const BMP280_Config_t *cfg)
{
    if (!hdev)
    {
        return BMP280_ERR_CONFIG;
    }
    hdev->initialized = 0;
    if (!hi2c || !cfg || cfg->os_temperature < BMP280_OS_1X ||
        cfg->os_temperature > BMP280_OS_16X || cfg->os_pressure < BMP280_OS_1X ||
        cfg->os_pressure > BMP280_OS_16X ||
        (cfg->mode != BMP280_MODE_NORMAL && cfg->mode != BMP280_MODE_FORCED) ||
        (unsigned)cfg->filter > BMP280_FILTER_16 ||
        (unsigned)cfg->standby > BMP280_STANDBY_4000MS || !isfinite(cfg->sea_level_hpa) ||
        cfg->sea_level_hpa <= 0)
    {
        return BMP280_ERR_CONFIG;
    }
    /* Maximum conversion time from oversampling, rounded up + 5 ms margin. */
    hdev->measurement_timeout_ms =
        (1825u + 2300u * ((1u << (cfg->os_temperature - 1u)) + (1u << (cfg->os_pressure - 1u))) +
         999u) /
            1000u +
        5u;
    /* Armazena recursos de hardware */
    hdev->hi2c = hi2c;
    hdev->dev_addr = (uint16_t)(addr_7bit << 1); /* HAL usa endereço 8 bits */
    hdev->sea_level_hpa = cfg->sea_level_hpa;
    hdev->t_fine = 0;
    hdev->temperature = 0.0f;
    hdev->pressure = 0.0f;
    hdev->altitude = 0.0f;

    BMP280_Status_t st;

    /* ── 1. Verifica presença e identidade do chip ───────────────────── */
    uint8_t chip_id = 0;
    st = _read_regs(hdev, REG_CHIP_ID, &chip_id, 1);
    if (st != BMP280_OK)
    {
        return BMP280_ERR_I2C;
    }
    if (chip_id != BMP280_CHIP_ID)
    {
        return BMP280_ERR_NOT_FOUND;
    }

    /* ── 2. Soft-reset para estado limpo ─────────────────────────────── */
    st = _write_reg(hdev, REG_RESET, RESET_VALUE);
    if (st != BMP280_OK)
    {
        return st;
    }
    HAL_Delay(10); /* Aguarda reinicialização do sensor (~2 ms conforme datasheet) */

    st = _wait_ready(hdev);
    if (st != BMP280_OK)
    {
        return st;
    }

    /* ── 3. Lê coeficientes de calibração da NVM ─────────────────────── */
    st = _read_calibration(hdev);
    if (st != BMP280_OK)
    {
        return st;
    }

    /* ── 4. Registrador config: standby + filtro IIR ─────────────────── */
    /*
     * Bit layout de REG_CONFIG [7:0]:
     *   [7:5] t_sb   — tempo de standby (modo Normal)
     *   [4:2] filter — coeficiente IIR
     *   [0]   spi3w_en — 0 = I²C (não alterar)
     */
    uint8_t reg_config = (uint8_t)((cfg->standby << 5) | (cfg->filter << 2));
    st = _write_reg(hdev, REG_CONFIG, reg_config);
    if (st != BMP280_OK)
    {
        return st;
    }

    /* ── 5. Registrador ctrl_meas: oversampling + modo ───────────────── */
    /*
     * Bit layout de REG_CTRL_MEAS [7:0]:
     *   [7:5] osrs_t — oversampling temperatura
     *   [4:2] osrs_p — oversampling pressão
     *   [1:0] mode   — Sleep / Forced / Normal
     */
    uint8_t reg_ctrl = (uint8_t)((cfg->os_temperature << 5) | (cfg->os_pressure << 2) | cfg->mode);
    st = _write_reg(hdev, REG_CTRL_MEAS, reg_ctrl);
    if (st != BMP280_OK)
    {
        return st;
    }

    hdev->expected_ctrl = reg_ctrl;
    hdev->expected_config = reg_config;
    st = _wait_ready(hdev);
    if (st != BMP280_OK)
    {
        return st;
    }
    st = _check_config(hdev);
    if (st != BMP280_OK)
    {
        return st;
    }
    hdev->initialized = 1;

    return BMP280_OK;
}

BMP280_Status_t BMP280_Read(BMP280_Handle_t *hdev)
{
    if (!hdev || !hdev->initialized)
    {
        return BMP280_ERR_CONFIG;
    }
    BMP280_Status_t st = _check_config(hdev);
    if (st != BMP280_OK)
    {
        return st;
    }
    if ((hdev->expected_ctrl & 3u) == BMP280_MODE_FORCED)
    {
        st = _write_reg(hdev, REG_CTRL_MEAS, hdev->expected_ctrl);
        if (st != BMP280_OK)
        {
            return st;
        }
        HAL_Delay(hdev->measurement_timeout_ms);
    }
    st = _wait_ready(hdev);
    if (st != BMP280_OK)
    {
        return st;
    }

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
    if (st != BMP280_OK)
    {
        return st;
    }

    int32_t adc_P =
        (int32_t)(((uint32_t)raw[0] << 12) | ((uint32_t)raw[1] << 4) | ((uint32_t)raw[2] >> 4));

    int32_t adc_T =
        (int32_t)(((uint32_t)raw[3] << 12) | ((uint32_t)raw[4] << 4) | ((uint32_t)raw[5] >> 4));

    if (adc_P == 0x80000 || adc_T == 0x80000)
    {
        return BMP280_ERR_RANGE;
    }
    BMP280_Handle_t candidate = *hdev;
    candidate.temperature = _compensate_temperature(&candidate, adc_T);
    candidate.pressure = _compensate_pressure(&candidate, adc_P);
    candidate.altitude = _calc_altitude(candidate.pressure, candidate.sea_level_hpa);
    if (!isfinite(candidate.temperature) || candidate.temperature < -40 ||
        candidate.temperature > 85 || !isfinite(candidate.pressure) || candidate.pressure < 300 ||
        candidate.pressure > 1100 || !isfinite(candidate.altitude))
    {
        return BMP280_ERR_RANGE;
    }
    st = _check_config(hdev);
    if (st != BMP280_OK)
    {
        return st;
    }
    *hdev = candidate;

    return BMP280_OK;
}

BMP280_Status_t BMP280_Reset(BMP280_Handle_t *hdev)
{
    if (!hdev)
    {
        return BMP280_ERR_CONFIG;
    }
    hdev->initialized = 0;
    return _write_reg(hdev, REG_RESET, RESET_VALUE);
}

BMP280_Status_t BMP280_IsConnected(BMP280_Handle_t *hdev)
{
    uint8_t chip_id = 0;
    BMP280_Status_t st = _read_regs(hdev, REG_CHIP_ID, &chip_id, 1);
    if (st != BMP280_OK)
    {
        return st;
    }
    if (chip_id != BMP280_CHIP_ID)
    {
        return BMP280_ERR_NOT_FOUND;
    }
    return BMP280_OK;
}

const char *BMP280_StatusStr(BMP280_Status_t status)
{
    switch (status)
    {
        case BMP280_OK:
            return "OK";
        case BMP280_ERR_NOT_FOUND:
            return "SENSOR NAO ENCONTRADO - Verifique endereco e fiacao I2C";
        case BMP280_ERR_I2C:
            return "FALHA I2C - Verifique SDA/SCL e resistores de pull-up";
        case BMP280_ERR_BUSY:
            return "TIMEOUT - Conversao nao concluiu";
        case BMP280_ERR_CONFIG:
            return "NAO INICIALIZADO / CONFIGURACAO PERDIDA";
        case BMP280_ERR_CALIBRATION:
            return "CALIBRACAO INVALIDA";
        case BMP280_ERR_RANGE:
            return "DADO / COMPENSACAO INVALIDA";
        default:
            return "ERRO DESCONHECIDO";
    }
}
