/**
 ******************************************************************************
 * @file    lora_sx1276.h
 * @brief   Driver mínimo para o SX1276 via SPI2 (barramento dedicado ao LoRa)
 *
 * Pinagem (definida em main.h / CubeMX):
 *   SPI2  → PB13 (SCK) | PB14 (MISO) | PB15 (MOSI)
 *   NSS   → PB12  (LORA_NSS_Pin / LORA_NSS_GPIO_Port)
 *   RESET → PB5  (LORA_RST_Pin / LORA_RST_GPIO_Port)
 *   DIO0  → PB4  (LORA_DIO0_Pin — TxDone / RxDone via EXTI4)
 *   DIO1  → PC3  (LORA_DIO1_Pin — RxTimeout via EXTI3)
 *
 * REGRA: este driver NÃO inclui sd_spi.h e NÃO usa hspi1.
 *        Todo acesso ao rádio passa exclusivamente por hspi2.
 ******************************************************************************
 */

#ifndef LORA_SX1276_H
#define LORA_SX1276_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"        /* pinos LORA_NSS, LORA_RST, LORA_DIO0, LORA_DIO1  */
#include "stm32f4xx_hal.h"
#include <stdbool.h>
#include <stdint.h>

/* ── Registrador de versão ──────────────────────────────────────────────── */
#define SX1276_REG_VERSION      0x42u   /**< Deve retornar 0x12 se o rádio
                                              está vivo e SPI está correto. */
#define SX1276_VERSION_EXPECTED 0x12u

/* ── Protótipos públicos ─────────────────────────────────────────────────── */

/**
 * @brief  Pulsa RESET do SX1276 (PB5) conforme datasheet §7.2.2:
 *         100 µs em LOW → soltar → aguardar 5 ms.
 *         Deve ser chamado antes de qualquer leitura de registrador.
 */
bool SX1276_Reset(void);

/**
 * @brief  Lê um registrador do SX1276 via SPI2.
 * @param  addr  Endereço do registrador (7 bits).
 * @retval Valor lido, ou 0x00 em caso de falha no HAL.
 */
uint8_t SX1276_ReadRegister(uint8_t addr);

/**
 * @brief  Escreve um registrador do SX1276 via SPI2.
 * @param  addr   Endereço do registrador (7 bits).
 * @param  value  Valor a escrever.
 */
void SX1276_WriteRegister(uint8_t addr, uint8_t value);

/**
 * @brief  Faz reset, lê REG_VERSION e verifica se é 0x12.
 * @param  version  [out] Valor bruto lido do registrador.
 * @retval true  → SX1276 detectado corretamente.
 *         false → barramento com problema (0xFF = MISO solto,
 *                 0x00 = rádio sem alimentação ou NSS errado).
 */
bool SX1276_CheckVersion(uint8_t *version);

#ifdef __cplusplus
}
#endif

#endif /* LORA_SX1276_H */
