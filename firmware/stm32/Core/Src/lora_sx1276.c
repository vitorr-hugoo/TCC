/** SX1276 via SPI2: configuracao verificada e conclusao LOCAL de TX, sem ACK. */
/**
 * @file lora_sx1276.c
 * @brief Implementa o transporte local LoRa SX1276 via SPI2.
 */

#include "lora_sx1276.h"
#include <string.h>
#include <stdio.h>

extern SPI_HandleTypeDef hspi2;
#define SX1276_SPI_TIMEOUT_MS  100u
#define SX1276_TX_TIMEOUT_MS   3000u
#define SX1276_MODE_TIMEOUT_MS 10u

typedef struct
{
    uint8_t addr, value;
} RegisterSetting;
static RegisterSetting settings[15];
static bool initialized;
static uint8_t mode_base = 0x80u;
static const char *last_error = "INIT";
static bool Fail(const char *reason)
{
    last_error = reason;
    return false;
}
const char *SX1276_LastError(void)
{
    return last_error;
}

static void LoRa_Select(void)
{
    HAL_GPIO_WritePin(LORA_NSS_GPIO_Port, LORA_NSS_Pin, GPIO_PIN_RESET);
}
static bool LoRa_Deselect(void)
{
    uint32_t start = HAL_GetTick();
    bool ready = true;
    while (__HAL_SPI_GET_FLAG(&hspi2, SPI_FLAG_BSY))
    {
        if ((uint32_t)(HAL_GetTick() - start) >= SX1276_SPI_TIMEOUT_MS)
        {
            ready = false;
            break;
        }
    }
    /* Liberar NSS mesmo em erro, sem outra espera ilimitada. */
    HAL_GPIO_WritePin(LORA_NSS_GPIO_Port, LORA_NSS_Pin, GPIO_PIN_SET);
    return ready ? true : Fail("SPI");
}
bool SX1276_ReadRegister(uint8_t addr, uint8_t *value)
{
    if (value == NULL)
    {
        return Fail("ARG");
    }
    uint8_t tx[2] = {(uint8_t)(addr & 0x7fu), 0};
    uint8_t rx[2] = {0};
    LoRa_Select();
    HAL_StatusTypeDef status = HAL_SPI_TransmitReceive(&hspi2, tx, rx, 2, SX1276_SPI_TIMEOUT_MS);
    bool released = LoRa_Deselect();
    if (status != HAL_OK || !released)
    {
        return Fail("SPI");
    }
    *value = rx[1];
    return true;
}
bool SX1276_WriteRegister(uint8_t addr, uint8_t value)
{
    uint8_t tx[2] = {(uint8_t)(addr | 0x80u), value};
    LoRa_Select();
    HAL_StatusTypeDef status = HAL_SPI_Transmit(&hspi2, tx, 2, SX1276_SPI_TIMEOUT_MS);
    bool released = LoRa_Deselect();
    return (status == HAL_OK && released) ? true : Fail("SPI");
}
static bool WriteBurst(const uint8_t *data, uint8_t len)
{
    uint8_t hdr = SX1276_REG_FIFO | 0x80u;
    LoRa_Select();
    HAL_StatusTypeDef status = HAL_SPI_Transmit(&hspi2, &hdr, 1, SX1276_SPI_TIMEOUT_MS);
    if (status == HAL_OK)
    {
        status = HAL_SPI_Transmit(&hspi2, (uint8_t *)data, len, SX1276_SPI_TIMEOUT_MS);
    }
    bool released = LoRa_Deselect();
    return (status == HAL_OK && released) ? true : Fail("SPI");
}
static bool Expect(uint8_t addr, uint8_t expected, const char *reason)
{
    uint8_t value;
    if (!SX1276_ReadRegister(addr, &value))
    {
        return false;
    }
    if (value == expected)
    {
        return true;
    }
    /* NSS has already been released. Report the first mismatch before
     * returning; do not retry or hide the failed verification. */
    printf("[LORA] %s: reg=0x%02X esperado=0x%02X lido=0x%02X\r\n", reason, (unsigned)addr,
           (unsigned)expected, (unsigned)value);
    return Fail(reason);
}
static bool WriteVerify(uint8_t addr, uint8_t value)
{
    if (!SX1276_WriteRegister(addr, value))
    {
        return false;
    }
    if (addr != SX1276_REG_OP_MODE)
    {
        return Expect(addr, value, "CONFIG");
    }

    /* A mode request is not an instantaneous static-register update.
     * Allow oscillator/mode startup, without rewriting or accepting Sleep
     * as Standby. SPI errors still abort immediately. */
    uint32_t start = HAL_GetTick();
    uint8_t actual = 0;
    for (unsigned attempt = 0; attempt <= SX1276_MODE_TIMEOUT_MS; ++attempt)
    {
        if (!SX1276_ReadRegister(addr, &actual))
        {
            return false;
        }
        if (actual == value)
        {
            return true;
        }
        if ((uint32_t)(HAL_GetTick() - start) >= SX1276_MODE_TIMEOUT_MS ||
            attempt == SX1276_MODE_TIMEOUT_MS)
        {
            break;
        }
        HAL_Delay(1);
    }
    printf("[LORA] CONFIG: reg=0x%02X esperado=0x%02X lido=0x%02X (transicao nao confirmou em %u "
           "ms)\r\n",
           (unsigned)addr, (unsigned)value, (unsigned)actual, (unsigned)SX1276_MODE_TIMEOUT_MS);
    return Fail("CONFIG");
}
bool SX1276_Reset(void)
{
    initialized = false;
    HAL_GPIO_WritePin(LORA_RST_GPIO_Port, LORA_RST_Pin, GPIO_PIN_RESET);
    HAL_Delay(2);
    HAL_GPIO_WritePin(LORA_RST_GPIO_Port, LORA_RST_Pin, GPIO_PIN_SET);
    HAL_Delay(10);
    return true;
}
/* Leitura somente: o reset do boot e uma operacao explicita separada. */
bool SX1276_CheckVersion(uint8_t *version)
{
    if (!SX1276_ReadRegister(SX1276_REG_VERSION, version))
    {
        return false;
    }
    return *version == SX1276_VERSION_EXPECTED ? true : Fail("RADIO");
}
bool SX1276_CheckHealth(void)
{
    last_error = "OK";
    uint8_t version;
    if (!SX1276_CheckVersion(&version))
    {
        return false;
    }
    if (!initialized)
    {
        return Fail("INIT");
    }
    if (!Expect(SX1276_REG_OP_MODE, (uint8_t)(mode_base | 1u), "CONFIG"))
    {
        if (strcmp(last_error, "CONFIG") == 0)
        {
            initialized = false;
        }
        return false;
    }
    for (unsigned i = 0; i < sizeof settings / sizeof settings[0]; ++i)
    {
        if (!Expect(settings[i].addr, settings[i].value, "CONFIG"))
        {
            /* Perda de configuracao exige init explicita. Falha SPI pode ser transitoria. */
            if (strcmp(last_error, "CONFIG") == 0)
            {
                initialized = false;
            }
            return false;
        }
    }
    return true;
}
static void TryStandby(void)
{
    const char *original_error = last_error;
    uint8_t version;
    if (SX1276_CheckVersion(&version))
    {
        (void)SX1276_WriteRegister(SX1276_REG_OP_MODE, (uint8_t)(mode_base | 1u));
    }
    last_error = original_error;
}
bool SX1276_Init(const SX1276_Config_t *cfg)
{
    initialized = false;
    last_error = "OK";
    if (cfg == NULL || cfg->frequency_hz < 137000000u || cfg->frequency_hz > 1020000000u ||
        cfg->spreading_factor < 7u || cfg->spreading_factor > 12u || cfg->bandwidth > 9u ||
        cfg->coding_rate < 1u || cfg->coding_rate > 4u || cfg->preamble_len < 6u ||
        cfg->tx_power_dbm < 2u || cfg->tx_power_dbm > 17u)
    {
        return Fail("ARG");
    }

    uint8_t version, old_mode;
    if (!SX1276_CheckVersion(&version) || !SX1276_ReadRegister(SX1276_REG_OP_MODE, &old_mode))
    {
        return false;
    }
    mode_base = (uint8_t)(0x80u | (cfg->frequency_hz < 525000000u ? 0x08u : 0u));
    /* LongRangeMode so pode mudar em sleep: entrar em sleep no modem atual. */
    if (!WriteVerify(SX1276_REG_OP_MODE, (uint8_t)(old_mode & 0xf8u)))
    {
        return false;
    }
    HAL_Delay(5);
    if (!WriteVerify(SX1276_REG_OP_MODE, mode_base))
    {
        return false;
    }
    uint32_t frf = (uint32_t)(((uint64_t)cfg->frequency_hz << 19) / 32000000u);
    static const uint32_t bandwidth_hz[] = {7800,  10400, 15600,  20800,  31250,
                                            41700, 62500, 125000, 250000, 500000};
    uint8_t mc3 =
        (uint8_t)(0x04u |
                  (((1u << cfg->spreading_factor) * 1000u > 16u * bandwidth_hz[cfg->bandwidth])
                       ? 0x08u
                       : 0u));
    RegisterSetting requested[] = {
        {SX1276_REG_FR_MSB, (uint8_t)(frf >> 16)},
        {SX1276_REG_FR_MID, (uint8_t)(frf >> 8)},
        {SX1276_REG_FR_LSB, (uint8_t)frf},
        {SX1276_REG_PA_CONFIG, (uint8_t)(0xf0u | (cfg->tx_power_dbm - 2u))},
        {SX1276_REG_PA_DAC, 0x84u},
        {SX1276_REG_OCP, 0x2bu},
        {SX1276_REG_FIFO_TX_BASE, 0},
        {SX1276_REG_FIFO_RX_BASE, 0},
        {SX1276_REG_MODEM_CONFIG1,
         (uint8_t)((cfg->bandwidth << 4) | (cfg->coding_rate << 1) | cfg->implicit_header)},
        {SX1276_REG_MODEM_CONFIG2,
         (uint8_t)((cfg->spreading_factor << 4) | (cfg->crc_enable ? 4u : 0u))},
        {SX1276_REG_MODEM_CONFIG3, mc3},
        {SX1276_REG_SYMB_TIMEOUT_LSB, 0x64u},
        {SX1276_REG_PREAMBLE_MSB, 0},
        {SX1276_REG_PREAMBLE_LSB, cfg->preamble_len},
        {SX1276_REG_IRQ_FLAGS_MASK, 0xf7u}};
    _Static_assert(sizeof requested == sizeof settings, "Configuracao incompleta");
    memcpy(settings, requested, sizeof settings);
    for (unsigned i = 0; i < sizeof settings / sizeof settings[0]; ++i)
    {
        if (!WriteVerify(settings[i].addr, settings[i].value))
        {
            TryStandby();
            return false;
        }
    }
    if (!SX1276_WriteRegister(SX1276_REG_IRQ_FLAGS, 0xffu) ||
        !Expect(SX1276_REG_IRQ_FLAGS, 0, "IRQ") ||
        !WriteVerify(SX1276_REG_OP_MODE, (uint8_t)(mode_base | 1u)))
    {
        TryStandby();
        return false;
    }
    initialized = true;
    if (!SX1276_CheckHealth())
    {
        initialized = false;
        return false;
    }
    return true;
}
bool SX1276_Transmit(const uint8_t *data, uint8_t len)
{
    last_error = "OK";
    if (data == NULL || len == 0u)
    {
        return Fail("ARG");
    }
    if (!SX1276_CheckHealth())
    {
        return false;
    }
    if (!WriteVerify(SX1276_REG_OP_MODE, (uint8_t)(mode_base | 1u)) ||
        !SX1276_WriteRegister(SX1276_REG_IRQ_FLAGS, 0xffu) ||
        !Expect(SX1276_REG_IRQ_FLAGS, 0, "IRQ") || !WriteVerify(SX1276_REG_FIFO_ADDR_PTR, 0) ||
        !WriteVerify(SX1276_REG_PAYLOAD_LENGTH, len) || !WriteBurst(data, len) ||
        !SX1276_WriteRegister(SX1276_REG_OP_MODE, (uint8_t)(mode_base | 3u)))
    {
        TryStandby();
        return false;
    }
    uint32_t start = HAL_GetTick();
    for (;;)
    {
        uint8_t flags;
        if (!SX1276_ReadRegister(SX1276_REG_IRQ_FLAGS, &flags))
        {
            break;
        }
        /* Sem RX, CAD ou FHSS nesta operacao: apenas TxDone e permitido. */
        if ((flags & (uint8_t)~SX1276_IRQ_TX_DONE) != 0u)
        {
            Fail("IRQ");
            break;
        }
        if ((uint32_t)(HAL_GetTick() - start) >= SX1276_TX_TIMEOUT_MS)
        {
            Fail("TIMEOUT");
            break;
        }
        if (flags & SX1276_IRQ_TX_DONE)
        {
            if (!SX1276_CheckHealth() ||
                !SX1276_WriteRegister(SX1276_REG_IRQ_FLAGS, SX1276_IRQ_TX_DONE) ||
                !Expect(SX1276_REG_IRQ_FLAGS, 0, "IRQ"))
            {
                break;
            }
            return true;
        }
    }
    TryStandby();
    return false;
}
