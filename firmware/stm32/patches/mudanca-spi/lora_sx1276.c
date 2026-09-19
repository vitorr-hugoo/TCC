/**
 ******************************************************************************
 * @file    lora_sx1276.c
 * @brief   Driver mínimo para o SX1276 via SPI2
 *
 * CORREÇÕES em relação à versão anterior:
 *  1. Removida a dependência de sd_spi.h / LoRa_Select() / SPI1.
 *  2. NSS controlado localmente com as macros de main.h.
 *  3. LoRa_Deselect() aguarda BSY do hspi2, não do hspi1.
 *  4. SX1276_Reset() usa os tempos corretos do datasheet (§7.2.2).
 ******************************************************************************
 */

#include "lora_sx1276.h"

/* ── Handle SPI2 (gerado pelo CubeMX, declarado em main.c) ─────────────── */
extern SPI_HandleTypeDef hspi2;

/* ── Constantes de protocolo SPI do SX1276 ──────────────────────────────── */
#define SX1276_READ_MASK    0x7Fu   /**< bit 7 = 0 → leitura  */
#define SX1276_WRITE_MASK   0x80u   /**< bit 7 = 1 → escrita  */
#define SX1276_SPI_TIMEOUT  100u    /**< ms — timeout HAL para cada transação */

/* ── Helpers de chip-select (privados, sem conflito com sd_spi) ─────────── */

/**
 * @brief Ativa o NSS do LoRa (PB12 → LOW).
 *        Usa as macros de main.h — mesma fonte de verdade do CubeMX.
 */
static inline void _LoRa_Select(void)
{
    HAL_GPIO_WritePin(LORA_NSS_GPIO_Port, LORA_NSS_Pin, GPIO_PIN_RESET);
}

/**
 * @brief Desativa o NSS do LoRa (PB12 → HIGH).
 *        Aguarda o flag BSY do SPI2 antes de soltar o NSS para garantir
 *        que o último clock já saiu no barramento.
 *
 * PITFALL: soltar o NSS enquanto BSY=1 trunca a transação SPI e faz o
 *          rádio ignorar o byte — causa típica de leituras sempre 0xFF.
 */
static inline void _LoRa_Deselect(void)
{
    while (__HAL_SPI_GET_FLAG(&hspi2, SPI_FLAG_BSY)) { /* busy-wait ~ns */ }
    HAL_GPIO_WritePin(LORA_NSS_GPIO_Port, LORA_NSS_Pin, GPIO_PIN_SET);
}

/* ── Implementação pública ───────────────────────────────────────────────── */

bool SX1276_Reset(void)
{
    /*
     * Sequência de reset conforme SX1276 datasheet §7.2.2:
     *   1. Levar RST a LOW por ≥ 100 µs.
     *   2. Soltar RST (HIGH ou floating — aqui HIGH via PP).
     *   3. Aguardar ≥ 5 ms para o rádio terminar o POR interno.
     *
     * HAL_Delay(1) ≈ 1 ms >> 100 µs → seguro.
     * HAL_Delay(10) dá margem extra para bancada com capacitância no RST.
     */
    HAL_GPIO_WritePin(LORA_RST_GPIO_Port, LORA_RST_Pin, GPIO_PIN_RESET);
    HAL_Delay(2);   /* > 100 µs */
    HAL_GPIO_WritePin(LORA_RST_GPIO_Port, LORA_RST_Pin, GPIO_PIN_SET);
    HAL_Delay(10);  /* > 5 ms — margem para POR */
    return true;
}

uint8_t SX1276_ReadRegister(uint8_t addr)
{
    /*
     * Protocolo SPI do SX1276 (full-duplex, 2 bytes):
     *   Byte 0 TX: endereço com bit 7 = 0 (leitura)
     *   Byte 1 TX: 0x00 (dummy para gerar clock)
     *   Byte 0 RX: ignorado (SX1276 ainda processa o endereço)
     *   Byte 1 RX: dado do registrador ← o que queremos
     */
    uint8_t tx[2] = { addr & SX1276_READ_MASK, 0x00u };
    uint8_t rx[2] = { 0x00u, 0x00u };

    _LoRa_Select();

    if (HAL_SPI_TransmitReceive(&hspi2, tx, rx, 2u, SX1276_SPI_TIMEOUT) != HAL_OK)
    {
        _LoRa_Deselect();
        return 0x00u;   /* falha HAL — verifique se MX_SPI2_Init foi chamado */
    }

    _LoRa_Deselect();
    return rx[1];   /* byte 1 contém o dado lido */
}

void SX1276_WriteRegister(uint8_t addr, uint8_t value)
{
    /*
     * Protocolo SPI do SX1276 (escrita, 2 bytes):
     *   Byte 0 TX: endereço com bit 7 = 1 (escrita)
     *   Byte 1 TX: valor a gravar
     */
    uint8_t tx[2] = { (uint8_t)(addr | SX1276_WRITE_MASK), value };

    _LoRa_Select();
    (void)HAL_SPI_Transmit(&hspi2, tx, 2u, SX1276_SPI_TIMEOUT);
    _LoRa_Deselect();
}

bool SX1276_CheckVersion(uint8_t *version)
{
    if (version == NULL)
    {
        return false;
    }

    SX1276_Reset();  /* garante POR limpo antes de ler o registrador */

    *version = SX1276_ReadRegister(SX1276_REG_VERSION);

    /*
     * 0x12 → SX1276 OK.
     * 0xFF → MISO solto/pull-up em alto ou NSS não abaixou.
     * 0x00 → rádio sem alimentação 3.3 V ou SPI2 não inicializado.
     *
     * Se retornar 0xFF, verifique:
     *  - MX_SPI2_Init() foi chamado antes de SX1276_CheckVersion()?
     *  - PB12 (LORA_NSS) foi configurado como GPIO Output PP pelo CubeMX?
     *  - O módulo LoRa está alimentado com 3.3 V?
     *  - PB13/14/15 estão configurados como AF SPI2 (AF5) no CubeMX?
     */
    return (*version == SX1276_VERSION_EXPECTED);
}
