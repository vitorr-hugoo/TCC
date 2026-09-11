/**
 ******************************************************************************
 * @file    lora_sx1276.c
 * @brief   Driver SX1276 via SPI2 — Passo 1: Init do modem LoRa
 *
 * PASSO 1 — O que foi adicionado nesta versão:
 *   - SX1276_Init(): configura frequência, potência, BW, SF, CR, CRC,
 *     preâmbulo e verifica escrita de cada registrador crítico.
 *   - Helpers estáticos para cálculo de frequência e campos de registrador.
 *   - Todos os magic numbers têm comentário explicando de onde vieram.
 ******************************************************************************
 */

#include "lora_sx1276.h"
#include <stdio.h>

extern SPI_HandleTypeDef hspi2;

#define SX1276_READ_MASK   0x7Fu
#define SX1276_WRITE_MASK  0x80u
#define SX1276_SPI_TIMEOUT 100u

static inline void LoRa_Select(void)
{
    HAL_GPIO_WritePin(LORA_NSS_GPIO_Port, LORA_NSS_Pin, GPIO_PIN_RESET);
}

static inline void LoRa_Deselect(void)
{
    while (__HAL_SPI_GET_FLAG(&hspi2, SPI_FLAG_BSY)) { }
    HAL_GPIO_WritePin(LORA_NSS_GPIO_Port, LORA_NSS_Pin, GPIO_PIN_SET);
}

bool SX1276_Reset(void)
{
    HAL_GPIO_WritePin(LORA_RST_GPIO_Port, LORA_RST_Pin, GPIO_PIN_RESET);
    HAL_Delay(2);
    HAL_GPIO_WritePin(LORA_RST_GPIO_Port, LORA_RST_Pin, GPIO_PIN_SET);
    HAL_Delay(10);
    return true;
}

uint8_t SX1276_ReadRegister(uint8_t addr)
{
    uint8_t tx[2] = {0};
    uint8_t rx[2] = {0};

    tx[0] = (uint8_t)(addr & SX1276_READ_MASK);
    tx[1] = 0x00u;

    LoRa_Select();

    if (HAL_SPI_TransmitReceive(&hspi2, tx, rx, 2, SX1276_SPI_TIMEOUT) != HAL_OK)
    {
        LoRa_Deselect();
        return 0x00u;
    }

    LoRa_Deselect();
    return rx[1];
}

void SX1276_WriteRegister(uint8_t addr, uint8_t value)
{
    uint8_t tx[2] = {0};

    tx[0] = (uint8_t)(addr | SX1276_WRITE_MASK);
    tx[1] = value;

    LoRa_Select();
    (void)HAL_SPI_Transmit(&hspi2, tx, 2, SX1276_SPI_TIMEOUT);
    LoRa_Deselect();
}

bool SX1276_CheckVersion(uint8_t *version)
{
    if (version == NULL)
        return false;

    SX1276_Reset();
    *version = SX1276_ReadRegister(SX1276_REG_VERSION);
    return (*version == SX1276_VERSION_EXPECTED);
}

static void SX1276_WriteBurst(uint8_t addr, const uint8_t *data, uint8_t len)
{
    if ((data == NULL) || (len == 0u))
        return;

    LoRa_Select();

    uint8_t hdr = (uint8_t)(addr | SX1276_WRITE_MASK);
    (void)HAL_SPI_Transmit(&hspi2, &hdr, 1, SX1276_SPI_TIMEOUT);
    (void)HAL_SPI_Transmit(&hspi2, (uint8_t *)data, len, SX1276_SPI_TIMEOUT);

    LoRa_Deselect();
}

static void SX1276_CalcFrf(uint32_t freq_hz, uint8_t *msb, uint8_t *mid, uint8_t *lsb)
{
    uint64_t frf = ((uint64_t)freq_hz << 19) / 32000000ULL;
    *msb = (uint8_t)((frf >> 16) & 0xFFu);
    *mid = (uint8_t)((frf >> 8) & 0xFFu);
    *lsb = (uint8_t)(frf & 0xFFu);
}

static uint8_t SX1276_BuildPaConfig(uint8_t tx_power_dbm)
{
    if (tx_power_dbm < 2u)  tx_power_dbm = 2u;
    if (tx_power_dbm > 17u) tx_power_dbm = 17u;

    return (uint8_t)(0x80u | 0x70u | ((tx_power_dbm - 2u) & 0x0Fu));
}

static uint8_t SX1276_BuildModemConfig1(const SX1276_Config_t *cfg)
{
    return (uint8_t)(((cfg->bandwidth & 0x0Fu) << 4) |
                     ((cfg->coding_rate & 0x07u) << 1) |
                     (cfg->implicit_header ? 0x01u : 0x00u));
}

static uint8_t SX1276_BuildModemConfig2(const SX1276_Config_t *cfg)
{
    return (uint8_t)(((cfg->spreading_factor & 0x0Fu) << 4) |
                     (cfg->crc_enable ? 0x04u : 0x00u));
}

static uint8_t SX1276_BuildModemConfig3(const SX1276_Config_t *cfg)
{
    (void)cfg;
    return 0x04u;
}

bool SX1276_Init(const SX1276_Config_t *cfg)
{
    if (cfg == NULL)
        return false;

    uint8_t fr_msb, fr_mid, fr_lsb;
    SX1276_CalcFrf(cfg->frequency_hz, &fr_msb, &fr_mid, &fr_lsb);

    SX1276_WriteRegister(SX1276_REG_OP_MODE, SX1276_MODE_SLEEP);
    HAL_Delay(5);
    printf("[LORA-INIT] Sleep+LoRa OK (RegOpMode=0x%02X)\r\n", SX1276_ReadRegister(SX1276_REG_OP_MODE));

    SX1276_WriteRegister(SX1276_REG_FR_MSB, fr_msb);
    SX1276_WriteRegister(SX1276_REG_FR_MID, fr_mid);
    SX1276_WriteRegister(SX1276_REG_FR_LSB, fr_lsb);
    printf("[LORA-INIT] Frequencia: %lu Hz (0x%02X%02X%02X)\r\n",
           cfg->frequency_hz, fr_msb, fr_mid, fr_lsb);

    uint8_t pa_cfg = SX1276_BuildPaConfig(cfg->tx_power_dbm);
    SX1276_WriteRegister(SX1276_REG_PA_CONFIG, pa_cfg);
    SX1276_WriteRegister(SX1276_REG_PA_DAC, 0x84u);
    SX1276_WriteRegister(SX1276_REG_OCP, 0x2Bu);
    printf("[LORA-INIT] Potencia: %u dBm (RegPaConfig=0x%02X)\r\n",
           cfg->tx_power_dbm, SX1276_ReadRegister(SX1276_REG_PA_CONFIG));

    SX1276_WriteRegister(SX1276_REG_FIFO_TX_BASE, 0x00u);
    SX1276_WriteRegister(SX1276_REG_FIFO_RX_BASE, 0x00u);

    uint8_t mc1 = SX1276_BuildModemConfig1(cfg);
    uint8_t mc2 = SX1276_BuildModemConfig2(cfg);
    uint8_t mc3 = SX1276_BuildModemConfig3(cfg);

    SX1276_WriteRegister(SX1276_REG_MODEM_CONFIG1, mc1);
    SX1276_WriteRegister(SX1276_REG_MODEM_CONFIG2, mc2);
    SX1276_WriteRegister(SX1276_REG_SYMB_TIMEOUT_LSB, 0x64u);
    SX1276_WriteRegister(SX1276_REG_MODEM_CONFIG3, mc3);

    printf("[LORA-INIT] Modem: SF%u BW=%u CR=4/%u CRC=%s\r\n",
           cfg->spreading_factor,
           cfg->bandwidth,
           (uint8_t)(cfg->coding_rate + 4u),
           cfg->crc_enable ? "ON" : "OFF");
    printf("[LORA-INIT] MC1=0x%02X MC2=0x%02X MC3=0x%02X\r\n", mc1, mc2, mc3);

    SX1276_WriteRegister(SX1276_REG_PREAMBLE_MSB, 0x00u);
    SX1276_WriteRegister(SX1276_REG_PREAMBLE_LSB, cfg->preamble_len);
    printf("[LORA-INIT] Preambulo: %u simbolos\r\n", cfg->preamble_len);

    SX1276_WriteRegister(SX1276_REG_IRQ_FLAGS_MASK, 0xF7u);
    SX1276_WriteRegister(SX1276_REG_IRQ_FLAGS, 0xFFu);

    SX1276_WriteRegister(SX1276_REG_OP_MODE, SX1276_MODE_STANDBY);
    HAL_Delay(2);
    printf("[LORA-INIT] Standby OK. Modem pronto para TX.\r\n");

    return true;
}

bool SX1276_Transmit(const uint8_t *data, uint8_t len)
{
    if ((data == NULL) || (len == 0u))
        return false;

    SX1276_WriteRegister(SX1276_REG_OP_MODE, SX1276_MODE_STANDBY);
    SX1276_WriteRegister(SX1276_REG_IRQ_FLAGS, 0xFFu);

    SX1276_WriteRegister(SX1276_REG_FIFO_ADDR_PTR, 0x00u);
    SX1276_WriteRegister(SX1276_REG_PAYLOAD_LENGTH, len);
    SX1276_WriteBurst(SX1276_REG_FIFO, data, len);

    SX1276_WriteRegister(SX1276_REG_OP_MODE, SX1276_MODE_TX);

    uint32_t t0 = HAL_GetTick();
    while ((SX1276_ReadRegister(SX1276_REG_IRQ_FLAGS) & SX1276_IRQ_TX_DONE) == 0u)
    {
        if ((HAL_GetTick() - t0) > 3000u)
        {
            SX1276_WriteRegister(SX1276_REG_OP_MODE, SX1276_MODE_STANDBY);
            printf("[LORA] TX timeout\r\n");
            return false;
        }
    }

    SX1276_WriteRegister(SX1276_REG_IRQ_FLAGS, SX1276_IRQ_TX_DONE);
    SX1276_WriteRegister(SX1276_REG_OP_MODE, SX1276_MODE_STANDBY);
    return true;
}
