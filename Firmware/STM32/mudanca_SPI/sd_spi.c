/**
 ******************************************************************************
 * @file    sd_spi.c
 * @brief   Implementação da camada SPI1 + DMA para o cartão SD
 *
 * CORREÇÕES em relação à versão anterior:
 *  1. Removidas LoRa_Select() e LoRa_Deselect() — pertencem a lora_sx1276.c.
 *  2. Removidas spi1_busy / SPI1_Take() / SPI1_Release() — não há mais
 *     compartilhamento do SPI1 com o LoRa; o risco de colisão deixou de
 *     existir com a separação em dois barramentos distintos.
 *  3. SD_Select() não precisa mais forçar LORA_CS_PORT HIGH — o LoRa tem
 *     barramento próprio (SPI2) e NSS próprio controlado por lora_sx1276.c.
 *  4. Callback HAL_SPI_TxRxCpltCallback verifica apenas SPI1 (não há risco
 *     de conflito com SPI2 porque o LoRa não usa DMA nesta fase).
 ******************************************************************************
 */

#include "sd_spi.h"
#include <string.h>     /* memset */

/* ── Buffers internos de DMA ─────────────────────────────────────────────── */
/*
 * PITFALL — Alinhamento:
 * __attribute__((aligned(4))) garante word-alignment, boa prática para DMA
 * mesmo que o STM32F446 não tenha CCM RAM restrita ao DMA.
 *
 * s_dma_tx_ff : TX de leitura — envia 0xFF para gerar clock sem dado real.
 * s_dma_rx_discard : RX de escrita — bytes recebidos durante escrita são
 *                    descartados (o SD não envia dados úteis durante CMD24).
 */
static uint8_t s_dma_tx_ff[512]      __attribute__((aligned(4)));
static uint8_t s_dma_rx_discard[512] __attribute__((aligned(4)));

/* Flag setada pela callback de conclusão de DMA */
static volatile uint8_t s_dma_done = 0;

/* ── Callback de conclusão de DMA ────────────────────────────────────────── */
/*
 * HAL_SPI_TxRxCpltCallback é uma função "weak" do HAL — esta implementação
 * a sobrescreve. Verificamos que é o SPI1 (SD) antes de setar a flag para
 * não interferir com outros periféricos SPI que possam usar DMA futuramente.
 */
void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == SPI1)
    {
        s_dma_done = 1;
    }
}

/* ── Chip Select do SD ───────────────────────────────────────────────────── */

/**
 * @brief Ativa o CS do SD (PB1 → LOW).
 *        Não mexe mais no NSS do LoRa — ele está em SPI2 e é gerenciado
 *        exclusivamente por lora_sx1276.c.
 */
void SD_Select(void)
{
    HAL_GPIO_WritePin(SD_CS_PORT, SD_CS_PIN, GPIO_PIN_RESET);
}

/**
 * @brief Desativa o CS do SD (PB1 → HIGH).
 */
void SD_Deselect(void)
{
    HAL_GPIO_WritePin(SD_CS_PORT, SD_CS_PIN, GPIO_PIN_SET);
}

/* ── API pública ─────────────────────────────────────────────────────────── */

void SPI1_DMA_Init(void)
{
    memset(s_dma_tx_ff, 0xFF, sizeof(s_dma_tx_ff));
}

void SPI1_SetSpeed(SPI1_Speed_t speed)
{
    __HAL_SPI_DISABLE(&hspi1);
    hspi1.Init.BaudRatePrescaler = (uint32_t)speed;
    HAL_SPI_Init(&hspi1);
    __HAL_SPI_ENABLE(&hspi1);
}

bool SD_Card_IsPresent(void)
{
    /*
     * No módulo MakerHero/Arduino o pino CD vai para GND quando o cartão
     * está inserido (chave mecânica normalmente aberta + pull-up interno).
     * GPIO_PIN_RESET = presente.
     */
    return (HAL_GPIO_ReadPin(SD_DETECT_PORT, SD_DETECT_PIN) == GPIO_PIN_RESET);
    /* DEBUG: troque por "return true;" apenas se o pino CD estiver com
     * problema de hardware e você precisar forçar a detecção. */
}

uint8_t SPI1_SendByte(uint8_t tx)
{
    uint8_t rx = 0u;
    HAL_SPI_TransmitReceive(&hspi1, &tx, &rx, 1u, SD_SPI_POLL_TIMEOUT);
    return rx;
}

void SPI1_SendBytes(const uint8_t *buf, uint16_t len)
{
    /* Casting de const necessário — HAL_SPI_Transmit não tem variante const.
     * O HAL não modifica o buffer de TX. */
    HAL_SPI_Transmit(&hspi1, (uint8_t *)buf, len, SD_SPI_POLL_TIMEOUT);
}

HAL_StatusTypeDef SPI1_DMA_ReadBlock(uint8_t *rxBuf)
{
    if (hspi1.State != HAL_SPI_STATE_READY)
    {
        HAL_SPI_Abort(&hspi1);
    }

    s_dma_done = 0u;

    HAL_StatusTypeDef ret = HAL_SPI_TransmitReceive_DMA(
                                &hspi1, s_dma_tx_ff, rxBuf, 512u);
    if (ret != HAL_OK) { return ret; }

    uint32_t t0 = HAL_GetTick();
    while (!s_dma_done)
    {
        if ((HAL_GetTick() - t0) >= SD_SPI_DMA_TIMEOUT)
        {
            HAL_SPI_Abort(&hspi1);
            return HAL_TIMEOUT;
        }
    }
    return HAL_OK;
}

HAL_StatusTypeDef SPI1_DMA_WriteBlock(const uint8_t *txBuf)
{
    if (hspi1.State != HAL_SPI_STATE_READY)
    {
        HAL_SPI_Abort(&hspi1);
    }

    s_dma_done = 0u;

    HAL_StatusTypeDef ret = HAL_SPI_TransmitReceive_DMA(
                                &hspi1, (uint8_t *)txBuf,
                                s_dma_rx_discard, 512u);
    if (ret != HAL_OK) { return ret; }

    uint32_t t0 = HAL_GetTick();
    while (!s_dma_done)
    {
        if ((HAL_GetTick() - t0) >= SD_SPI_DMA_TIMEOUT)
        {
            HAL_SPI_Abort(&hspi1);
            return HAL_TIMEOUT;
        }
    }
    return HAL_OK;
}
