/**
 ******************************************************************************
 * @file    sd_card.h
 * @brief   Camada 2 — Protocolo SD sobre SPI
 *
 * Responsabilidades:
 *   - Sequência de inicialização do cartão (CMD0 → CMD8 → ACMD41 → CMD58).
 *   - Leitura e escrita de blocos de 512 bytes (CMD17 / CMD24) usando DMA.
 *   - Suporte a SD v1 (byte addressing), SD v2 e SDHC/SDXC (block addressing).
 *
 * Esta camada consome exclusivamente a camada sd_spi (nunca acessa hspi1
 * diretamente) e é consumida pela camada FatFs (user_diskio.c).
 ******************************************************************************
 */

#ifndef SD_CARD_H
#define SD_CARD_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "stm32f4xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

    /* ─── Tipo de retorno ────────────────────────────────────────────────────── */
    typedef enum
    {
        SD_OK = 0x00,
        SD_ERR_NOT_RESPONDING = 0x01, /**< Cartão/SPI não respondeu          */
        SD_ERR_TIMEOUT = 0x02,        /**< Timeout aguardando resposta       */
        SD_ERR_INIT = 0x03,           /**< Falha na sequência de init       */
        SD_ERR_IO = 0x04,             /**< Erro de leitura/escrita          */
        SD_ERR_PARAM = 0x05,          /**< Parâmetro ou setor inválido      */
    } SD_Status_t;

    /* ─── Tipo de cartão detectado ───────────────────────────────────────────── */
    typedef enum
    {
        SD_TYPE_UNKNOWN = 0,
        SD_TYPE_V1,   /**< SD v1.x — endereçamento por byte                */
        SD_TYPE_V2,   /**< SD v2.0 SDSC — endereçamento por byte           */
        SD_TYPE_SDHC, /**< SDHC/SDXC — endereçamento por bloco (512 bytes) */
    } SD_CardType_t;

    /* ─── Handle do driver SD ────────────────────────────────────────────────── */
    typedef struct
    {
        SD_CardType_t type;    /**< Tipo detectado na inicialização      */
        bool initialized;      /**< true após SD_SPI_InitCard com sucesso */
        uint32_t sector_count; /**< Total de setores de 512 bytes        */
    } SD_Handle_t;

    /* ─── Protótipos ─────────────────────────────────────────────────────────── */

    /**
 * @brief  Inicializa o cartão SD no modo SPI.
 *
 * Sequência:
 *   1. Verifica presença do cartão (CD pin).
 *   2. SPI lento (~400 kHz) + 80 pulsos de clock com CS alto.
 *   3. CMD0 → entra no modo SPI (R1 = 0x01).
 *   4. CMD8 → detecta versão do cartão (SD v1/v2).
 *   5. ACMD41 (com HCS=1 para SD v2) em loop até R1 = 0x00.
 *   6. CMD58 → lê OCR; bit CCS = 1 indica SDHC/SDXC.
 *   7. CMD16 → força bloco de 512 bytes (somente para SD v1 e SD v2 SDSC).
 *   8. SPI rápido (~11,25 MHz).
 *
 * @param  hsd  Ponteiro para o handle — preenchido pela função.
 * @retval SD_OK | SD_ERR_NOT_RESPONDING | SD_ERR_TIMEOUT | SD_ERR_INIT
 */
    SD_Status_t SD_SPI_InitCard(SD_Handle_t *hsd);

    /**
 * @brief  Lê um bloco de 512 bytes do SD via DMA.
 *
 * @param  hsd     Handle inicializado.
 * @param  sector  Número do setor lógico (LBA).
 * @param  buffer  Buffer de destino (512 bytes, 4-byte aligned).
 * @retval SD_OK | SD_ERR_NOT_RESPONDING | SD_ERR_TIMEOUT | SD_ERR_IO
 */
    SD_Status_t SD_SPI_ReadBlock(SD_Handle_t *hsd, uint32_t sector, uint8_t *buffer);

    /**
 * @brief  Escreve um bloco de 512 bytes no SD via DMA.
 *
 * @param  hsd     Handle inicializado.
 * @param  sector  Número do setor lógico (LBA).
 * @param  buffer  Buffer de origem (512 bytes, 4-byte aligned).
 * @retval SD_OK | SD_ERR_NOT_RESPONDING | SD_ERR_TIMEOUT | SD_ERR_IO
 */
    SD_Status_t SD_SPI_WriteBlock(SD_Handle_t *hsd, uint32_t sector, const uint8_t *buffer);

    /** Aguarda o cartão terminar qualquer operação interna pendente. */
    SD_Status_t SD_SPI_Sync(SD_Handle_t *hsd);

    /**
 * @brief  Retorna string descritiva do código de status.
 */
    const char *SD_StatusStr(SD_Status_t status);

#ifdef __cplusplus
}
#endif
#endif /* SD_CARD_H */
