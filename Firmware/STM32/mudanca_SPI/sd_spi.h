/**
 ******************************************************************************
 * @file    sd_spi.h
 * @brief   Camada 1 — Controle SPI1 + DMA exclusivo para o cartão SD
 *
 * RESPONSABILIDADES DESTA CAMADA:
 *   - Controlar o CS do SD (PB1) de forma exclusiva.
 *   - Detectar presença do cartão via PC4.
 *   - Reconfigurar velocidade do SPI1 (lento na init, rápido na operação).
 *   - Transferências polling para comandos curtos (1–8 bytes).
 *   - Transferências DMA para blocos de 512 bytes.
 *
 * O QUE FOI REMOVIDO (correção):
 *   - LoRa_Select() / LoRa_Deselect() → movidas para lora_sx1276.c (privadas)
 *   - LORA_CS_PORT / LORA_CS_PIN      → usar LORA_NSS_* de main.h
 *   - SPI1_SPEED_LORA                 → SPI2 tem velocidade própria no CubeMX
 *   - spi1_busy / SPI1_Take / SPI1_Release → não há mais compartilhamento
 *
 * Barramento SD (SPI1 — exclusivo):
 *   PA5 → SCK  |  PA6 → MISO  |  PA7 → MOSI
 *   PB1 → SD_CS (ativo em LOW)
 *   PC4 → CD   (LOW = cartão presente no módulo MakerHero)
 *
 * Barramento LoRa (SPI2 — gerenciado por lora_sx1276.c):
 *   PB13 → SCK  |  PB14 → MISO  |  PB15 → MOSI
 *   PB0  → LORA_NSS (gerenciado por lora_sx1276.c, NÃO por sd_spi)
 ******************************************************************************
 */

#ifndef SD_SPI_H
#define SD_SPI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"
#include <stdbool.h>
#include <stdint.h>

/* ── Handle externo (gerado pelo CubeMX) ─────────────────────────────────── */
extern SPI_HandleTypeDef hspi1;

/* ── Pinos do SD ─────────────────────────────────────────────────────────── */
#define SD_CS_PORT      GPIOB
#define SD_CS_PIN       GPIO_PIN_1

#define SD_DETECT_PORT  GPIOC
#define SD_DETECT_PIN   GPIO_PIN_4

/* ── Timeouts (ms) ───────────────────────────────────────────────────────── */
#define SD_SPI_POLL_TIMEOUT  500u   /**< Polling de byte único              */
#define SD_SPI_DMA_TIMEOUT  2000u   /**< Transferência DMA de 512 bytes     */

/* ── Velocidades do SPI1 ─────────────────────────────────────────────────── */
/**
 * SLOW : fPCLK2/256 → ~703 kHz @ 90 MHz APB2.
 *         Obrigatório na inicialização do SD (protocolo exige < 400 kHz).
 * FAST : fPCLK2/8   → ~22,5 MHz. Usado após init bem-sucedida.
 *
 * Nota: SPI2 (LoRa) tem seu próprio prescaler configurado no CubeMX
 *       (SPIBAUDRATEPRESCALER_128 → ~703 kHz @ APB1 45 MHz).
 *       Não há mais SPI1_SPEED_LORA aqui.
 */
typedef enum {
    SPI1_SPEED_SLOW = SPI_BAUDRATEPRESCALER_256,
    SPI1_SPEED_FAST = SPI_BAUDRATEPRESCALER_8,
} SPI1_Speed_t;

/* ── Chip Select do SD ───────────────────────────────────────────────────── */
void SD_Select(void);
void SD_Deselect(void);

/* ── API pública ─────────────────────────────────────────────────────────── */

/**
 * @brief Inicializa buffers internos de DMA (preenche TX dummy com 0xFF).
 *        Chamar uma vez em Storage_Init antes de qualquer operação SD.
 */
void SPI1_DMA_Init(void);

/**
 * @brief Reconfigura o prescaler do SPI1 sem reinicializar o periférico.
 * @param speed  SPI1_SPEED_SLOW (init SD) ou SPI1_SPEED_FAST (operação).
 */
void SPI1_SetSpeed(SPI1_Speed_t speed);

/**
 * @brief Verifica se um cartão SD está inserido (PC4).
 * @retval true se presente, false se ausente.
 */
bool SD_Card_IsPresent(void);

/**
 * @brief Envia e recebe 1 byte via SPI1 em modo polling.
 * @param tx  Byte a transmitir (0xFF = dummy para forçar clock).
 * @retval    Byte recebido pelo MISO.
 */
uint8_t SPI1_SendByte(uint8_t tx);

/**
 * @brief Transmite um buffer de bytes via SPI1 em modo polling.
 */
void SPI1_SendBytes(const uint8_t *buf, uint16_t len);

/**
 * @brief Lê exatamente 512 bytes do SD via DMA (TX = 0xFF dummy).
 *        CS deve estar ativo (SD_Select) antes de chamar.
 * @param rxBuf  Buffer de destino — DEVE ser 4-byte aligned para DMA.
 * @retval HAL_OK | HAL_TIMEOUT | HAL_ERROR
 */
HAL_StatusTypeDef SPI1_DMA_ReadBlock(uint8_t *rxBuf);

/**
 * @brief Escreve exatamente 512 bytes no SD via DMA.
 *        CS deve estar ativo (SD_Select) antes de chamar.
 * @param txBuf  Buffer de origem — DEVE ser 4-byte aligned para DMA.
 * @retval HAL_OK | HAL_TIMEOUT | HAL_ERROR
 */
HAL_StatusTypeDef SPI1_DMA_WriteBlock(const uint8_t *txBuf);

#ifdef __cplusplus
}
#endif

#endif /* SD_SPI_H */
