/**
 ******************************************************************************
 * @file    bh1750.c
 * @brief   Implementação do driver BH1750 (ROHM Semiconductor)
 *
 * Referência: BH1750FVI Datasheet — Rev. D (ROHM, 2011)
 *
 * ATENÇÃO — Protocolo I²C diferente do BMP280:
 *   O BH1750 não possui registradores internos endereçáveis.
 *   Toda comunicação é feita com comandos diretos (sem endereço de reg):
 *     TX: [ START | dev_addr W | comando | STOP ]
 *     RX: [ START | dev_addr R | byte_MSB | byte_LSB | STOP ]
 *   Por isso usamos HAL_I2C_Master_Transmit / Receive, e NÃO Mem_Read/Write.
 ******************************************************************************
 */

#include "bh1750.h"

/* ─── Tempos de medição por modo (ms) — datasheet tabela 1 ──────────────── */
/*
 * Adicionamos ~50% de margem sobre o valor típico do datasheet para garantir
 * que o dado esteja pronto mesmo no pior caso (temperatura, tensão).
 *   H-Resolution típico: 120 ms → usamos 180 ms
 *   L-Resolution típico:  16 ms → usamos  24 ms
 */
#define MEAS_TIME_H_MS   180u
#define MEAS_TIME_L_MS    24u

/* ─── Helper: envia um comando de 1 byte ao sensor ──────────────────────── */
static BH1750_Status_t _send_cmd(BH1750_Handle_t *hdev, uint8_t cmd)
{
    HAL_StatusTypeDef ret = HAL_I2C_Master_Transmit(
        hdev->hi2c,
        hdev->dev_addr,
        &cmd,
        1,
        BH1750_I2C_TIMEOUT);

    return (ret == HAL_OK) ? BH1750_OK : BH1750_ERR_I2C;
}

/* ─── Helper: retorna o tempo de medição do modo atual ──────────────────── */
static uint32_t _meas_time_ms(BH1750_Mode_t mode)
{
    switch (mode) {
        case BH1750_MODE_CONT_L_RES:
        case BH1750_MODE_ONCE_L_RES:
            return MEAS_TIME_L_MS;
        default:
            return MEAS_TIME_H_MS;
    }
}

/* ─── API pública ────────────────────────────────────────────────────────── */

BH1750_Status_t BH1750_Init(BH1750_Handle_t   *hdev,
                             I2C_HandleTypeDef *hi2c,
                             uint8_t            addr_7bit,
                             BH1750_Mode_t      mode)
{
    hdev->hi2c        = hi2c;
    hdev->dev_addr    = (uint16_t)(addr_7bit << 1);
    hdev->mode        = mode;
    hdev->k_factor    = BH1750_DEFAULT_K_FACTOR;
    hdev->illuminance = 0.0f;
    hdev->irradiance  = 0.0f;

    BH1750_Status_t st;

    /* 1. Power On */
    st = _send_cmd(hdev, BH1750_CMD_POWER_ON);
    if (st != BH1750_OK) return st;

    /* 2. Reset do registrador de dados (recomendado após power-on) */
    st = _send_cmd(hdev, BH1750_CMD_RESET);
    if (st != BH1750_OK) return st;

    /* 3. Inicia o modo de medição */
    st = _send_cmd(hdev, (uint8_t)mode);
    if (st != BH1750_OK) return st;

    /*
     * Em modo Continuous, o sensor já começou a medir. O chamador deve
     * aguardar _meas_time_ms() antes da primeira chamada a BH1750_Read.
     * Em modo One Time, BH1750_Read controla o timing internamente.
     */
    return BH1750_OK;
}

BH1750_Status_t BH1750_Read(BH1750_Handle_t *hdev)
{
    BH1750_Status_t st;

    /*
     * Em modo One Time: o sensor volta a Power Down após cada medição.
     * É necessário reenviar o comando de medição e aguardar o tempo
     * necessário antes de ler — tudo feito aqui de forma bloqueante.
     *
     * Em modo Continuous: o sensor já tem um resultado fresco disponível
     * (desde que tenha passado ao menos um ciclo completo após o Init).
     * Basta ler os 2 bytes diretamente.
     */
    if (hdev->mode == BH1750_MODE_ONCE_H_RES  ||
        hdev->mode == BH1750_MODE_ONCE_H_RES2 ||
        hdev->mode == BH1750_MODE_ONCE_L_RES)
    {
        /* Reativa e dispara uma medição */
        st = _send_cmd(hdev, BH1750_CMD_POWER_ON);
        if (st != BH1750_OK) return st;

        st = _send_cmd(hdev, (uint8_t)hdev->mode);
        if (st != BH1750_OK) return st;

        /* Aguarda a medição concluir */
        HAL_Delay(_meas_time_ms(hdev->mode));
    }

    /* Lê os 2 bytes de resultado (big-endian: MSB primeiro) */
    uint8_t raw[2] = {0};
    HAL_StatusTypeDef ret = HAL_I2C_Master_Receive(
        hdev->hi2c,
        hdev->dev_addr,
        raw,
        2,
        BH1750_I2C_TIMEOUT);

    if (ret != HAL_OK) return BH1750_ERR_I2C;

    /*
     * Conversão do valor bruto para lux:
     *
     *   raw_word = (MSB << 8) | LSB
     *
     *   Modo H-Resolution  → lux = raw_word / 1.2
     *   Modo H-Resolution2 → lux = raw_word / 2.4  (dobra a resolução angular)
     *   Modo L-Resolution  → lux = raw_word / 1.2
     *
     * Faixa máxima: 65535 raw → ~54612 lux (H-Res) / ~27306 lux (H-Res2).
     * Luz solar direta pode atingir 100.000+ lux → para medição em campo
     * aberto, H-Resolution simples (divisor 1.2) é o mais indicado.
     */
    uint16_t raw_word = (uint16_t)((raw[0] << 8) | raw[1]);
    float divisor = (hdev->mode == BH1750_MODE_CONT_H_RES2 ||
                     hdev->mode == BH1750_MODE_ONCE_H_RES2) ? 2.4f : 1.2f;

    hdev->illuminance = (float)raw_word / divisor;

    /*
     * Estimativa de irradiância solar global (W/m²):
     *   irradiância = iluminância [lux] × k_factor [W·m⁻²·lux⁻¹]
     *
     * O k_factor padrão (1/126,7) é válido para luz solar direta ao meio-dia.
     * Após calibração experimental com a estação de referência da UFTM,
     * use BH1750_SetCalibration() para ajustar o valor.
     *
     * Limitação conhecida: o BH1750 mede iluminância fotométrica (ponderada
     * pela sensibilidade do olho humano, pico ~555 nm), enquanto a irradiância
     * solar cobre o espectro completo (280–2500 nm). A conversão é uma
     * aproximação — documente essa limitação no TCC conforme a arquitetura
     * do projeto prevê.
     */
    hdev->irradiance = hdev->illuminance * hdev->k_factor;

    return BH1750_OK;
}

void BH1750_SetCalibration(BH1750_Handle_t *hdev, float k_factor)
{
    hdev->k_factor = k_factor;
}

BH1750_Status_t BH1750_PowerDown(BH1750_Handle_t *hdev)
{
    return _send_cmd(hdev, BH1750_CMD_POWER_DOWN);
}

const char *BH1750_StatusStr(BH1750_Status_t status)
{
    switch (status) {
        case BH1750_OK:          return "OK";
        case BH1750_ERR_I2C:     return "FALHA I2C - Verifique SDA/SCL e endereco (ADDR pin)";
        case BH1750_ERR_NOT_RDY: return "DADO NAO DISPONIVEL - Aguarde o tempo de medicao";
        default:                 return "ERRO DESCONHECIDO";
    }
}
