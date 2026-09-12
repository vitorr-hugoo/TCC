/**
 ******************************************************************************
 * @file    sd_card.c
 * @brief   Implementação do protocolo SD sobre SPI
 *
 * Referência: Physical Layer Simplified Specification v8.00 (SD Association)
 *   Seção 7 — SPI Mode
 ******************************************************************************
 */

#include "sd_card.h"
#include "sd_spi.h"
#include <string.h>
#include <stdio.h>

/* ─── Comandos SD ────────────────────────────────────────────────────────── */
#define SD_CMD0   0u  /**< GO_IDLE_STATE       — Reset para modo SPI       */
#define SD_CMD8   8u  /**< SEND_IF_COND        — Verifica tensão/versão    */
#define SD_CMD9   9u  /**< SEND_CSD             — Lê capacidade real       */
#define SD_CMD16  16u /**< SET_BLOCKLEN         — Força bloco de 512 bytes  */
#define SD_CMD17  17u /**< READ_SINGLE_BLOCK    — Lê 1 setor               */
#define SD_CMD24  24u /**< WRITE_BLOCK          — Escreve 1 setor          */
#define SD_CMD55  55u /**< APP_CMD              — Prefixo para ACMD        */
#define SD_CMD58  58u /**< READ_OCR             — Lê registrador de operação */
#define SD_ACMD41 41u /**< SD_SEND_OP_COND      — Inicia inicialização      */

/* ─── Bits de resposta R1 ────────────────────────────────────────────────── */
#define R1_IDLE_STATE  0x01u
#define R1_ILLEGAL_CMD 0x04u
#define R1_READY       0x00u

/* ─── Tokens de dados ────────────────────────────────────────────────────── */
#define TOKEN_START_BLOCK 0xFEu /**< Início de bloco de leitura/escrita    */
#define DATA_RESP_MASK    0x1Fu /**< Máscara da resposta de escrita        */
#define DATA_RESP_OK      0x05u /**< Dados aceitos pelo cartão             */

/* ─── Timeouts ───────────────────────────────────────────────────────────── */
//#define INIT_RETRY_MAX   1000u   /**< Iterações máx. para ACMD41 (~1 s)      */
//#define TOKEN_WAIT_MAX   200u    /**< ms máximos aguardando token de dado    */
//#define BUSY_WAIT_MAX    500u    /**< ms máximos aguardando fim de escrita   */

#define INIT_RETRY_MAX 2000u /**< Iterações máx. para ACMD41 (~1 s)      */
#define TOKEN_WAIT_MAX 1000u /**< ms máximos aguardando token de dado    */
#define BUSY_WAIT_MAX  2000u /**< ms máximos aguardando fim de escrita   */

/* ─── Helpers internos ───────────────────────────────────────────────────── */

/**
 * @brief  Envia um comando SD e retorna o byte R1 de resposta.
 *
 * Formato de comando (6 bytes):
 *   [0] = 0x40 | cmd
 *   [1..4] = argumento (big-endian)
 *   [5] = CRC7 | 0x01
 *
 * CRC é validado apenas no CMD0 (0x95) e CMD8 (0x87).
 * Após entrar no modo SPI com CMD0, o CRC é desabilitado e 0xFF é aceito.
 */
static uint8_t _send_cmd(uint8_t cmd, uint32_t arg, uint8_t crc)
{
    uint8_t buf[6];
    buf[0] = 0x40u | cmd;
    buf[1] = (uint8_t)(arg >> 24);
    buf[2] = (uint8_t)(arg >> 16);
    buf[3] = (uint8_t)(arg >> 8);
    buf[4] = (uint8_t)(arg);
    buf[5] = crc;

    /* 1 byte dummy entre o fim de uma resposta e o próximo comando */
    SPI1_SendByte(0xFF);
    SPI1_SendBytes(buf, 6);

    /*
     * Aguarda resposta R1 (bit 7 = 0, de acordo com o protocolo SD).
     * O cartão pode inserir até 8 bytes de espera antes de responder.
     */
    uint8_t r1 = 0xFF;
    for (int i = 0; i < 8; i++)
    {
        r1 = SPI1_SendByte(0xFF);
        if (!(r1 & 0x80u))
        {
            break; /* bit 7 = 0 → resposta válida */
        }
    }
    return r1;
}

/**
 * @brief  Aguarda o cartão sair do estado ocupado (MISO = 0xFF).
 *         Chamado após escrita de bloco para não corromper os dados.
 *
 * @param  timeout_ms  Tempo máximo de espera.
 * @retval true se livre, false se timeout.
 */
static bool _wait_not_busy(uint32_t timeout_ms)
{
    uint32_t t0 = HAL_GetTick();
    while (SPI1_SendByte(0xFF) != 0xFF)
    {
        if ((HAL_GetTick() - t0) >= timeout_ms)
        {
            return false;
        }
    }
    return true;
}

/**
 * @brief  Aguarda o token de início de bloco (0xFE) antes de ler dados.
 * @retval true se token recebido, false se timeout ou token de erro.
 */
static bool _wait_start_token(void)
{
    uint32_t t0 = HAL_GetTick();
    uint8_t token;
    while ((token = SPI1_SendByte(0xFF)) == 0xFF)
    {
        if ((HAL_GetTick() - t0) >= TOKEN_WAIT_MAX)
        {
            return false;
        }
    }
    return (token == TOKEN_START_BLOCK);
}

static SD_Status_t _abort_transaction(SD_Handle_t *hsd, SD_Status_t status)
{
    SD_Deselect();
    (void)SPI1_SendByte(0xFFu);
    if (hsd != NULL)
    {
        hsd->initialized = false;
    }
    return status;
}

static SD_Status_t _read_csd(SD_Handle_t *hsd)
{
    uint8_t csd[16];
    uint8_t r1 = _send_cmd(SD_CMD9, 0x00000000u, 0xFFu);

    if (r1 != R1_READY)
    {
        return SD_ERR_IO;
    }
    if (!_wait_start_token())
    {
        return SD_ERR_TIMEOUT;
    }

    for (uint8_t i = 0u; i < sizeof(csd); i++)
    {
        csd[i] = SPI1_SendByte(0xFFu);
    }
    (void)SPI1_SendByte(0xFFu); /* CRC */
    (void)SPI1_SendByte(0xFFu);

    uint8_t csd_structure = (uint8_t)((csd[0] >> 6) & 0x03u);
    uint64_t sectors = 0u;

    if (csd_structure == 1u)
    {
        /* CSD v2.0 (SDHC/SDXC): capacidade = (C_SIZE + 1) * 1024 setores. */
        uint32_t c_size =
            ((uint32_t)(csd[7] & 0x3Fu) << 16) | ((uint32_t)csd[8] << 8) | (uint32_t)csd[9];
        sectors = ((uint64_t)c_size + 1u) * 1024u;
    }
    else if (csd_structure == 0u)
    {
        /* CSD v1.0 (SDSC). Converte a capacidade em blocos de 512 bytes. */
        uint8_t read_bl_len = (uint8_t)(csd[5] & 0x0Fu);
        uint16_t c_size = (uint16_t)(((uint16_t)(csd[6] & 0x03u) << 10) | ((uint16_t)csd[7] << 2) |
                                     ((uint16_t)csd[8] >> 6));
        uint8_t c_size_mult = (uint8_t)(((csd[9] & 0x03u) << 1) | ((csd[10] >> 7) & 0x01u));
        uint64_t block_count = ((uint64_t)c_size + 1u) << (c_size_mult + 2u);
        uint64_t block_length = 1ULL << read_bl_len;
        sectors = (block_count * block_length) / 512u;
    }
    else
    {
        return SD_ERR_INIT;
    }

    if ((sectors == 0u) || (sectors > UINT32_MAX))
    {
        return SD_ERR_INIT;
    }

    hsd->sector_count = (uint32_t)sectors;
    return SD_OK;
}

/* ─── API pública ────────────────────────────────────────────────────────── */

SD_Status_t SD_SPI_InitCard(SD_Handle_t *hsd)
{
    if (hsd == NULL)
    {
        return SD_ERR_PARAM;
    }

    hsd->initialized = false;
    hsd->type = SD_TYPE_UNKNOWN;
    hsd->sector_count = 0;

    printf("\r\n--- INICIANDO INIT DO SD ---\r\n");

    /* 1. SPI lento + 80 pulsos de clock com CS alto */
    HAL_Delay(500);
    SD_Deselect();

    if (SPI1_SetSpeed(SPI1_SPEED_SLOW) != HAL_OK)
    {
        printf("[SD] Falha ao configurar SPI1 em baixa velocidade.\r\n");
        return SD_ERR_INIT;
    }

    printf("Enviando 80 dummy clocks...\r\n");
    for (uint8_t i = 0u; i < 10u; i++)
    {
        (void)SPI1_SendByte(0xFFu);
    }
    HAL_Delay(20);

    /* 2. CMD0 — Reset para modo SPI (R1 deve ser 0x01) */
    SD_Select(); // Abaixa o CS UMA VEZ e mantém

    printf("Enviando CMD0 (Reset SPI)...\r\n");
    uint8_t r1 = 0xFF;
    for (int i = 0; i < 10; i++)
    {
        r1 = _send_cmd(SD_CMD0, 0x00000000, 0x95);
        printf("Tentativa %d, Resposta do CMD0: 0x%02X\r\n", i + 1, r1);
        if (r1 == R1_IDLE_STATE)
        {
            printf("-> SUCESSO: CMD0 respondeu 0x01\r\n");
            break;
        }
        HAL_Delay(10);
    }

    if (r1 != R1_IDLE_STATE)
    {
        printf("[SD] CMD0 sem resposta valida: 0x%02X\r\n", r1);
        return _abort_transaction(hsd, SD_ERR_NOT_RESPONDING);
    }

    /* 3. CMD8 — Detecta versão do cartão */
    printf("Enviando CMD8...\r\n");
    r1 = _send_cmd(SD_CMD8, 0x000001AA, 0x87);
    bool is_v2 = false;
    if (r1 == R1_IDLE_STATE)
    {
        uint8_t r7[4];
        for (int i = 0; i < 4; i++)
        {
            r7[i] = SPI1_SendByte(0xFF);
        }
        if ((r7[2] == 0x01) && (r7[3] == 0xAA))
        {
            printf("-> Cartao V2: Tensao aceita.\r\n");
            is_v2 = true;
        }
        else
        {
            return _abort_transaction(hsd, SD_ERR_INIT);
        }
    }
    else if ((r1 & R1_ILLEGAL_CMD) == 0u)
    {
        printf("[SD] Resposta inesperada ao CMD8: 0x%02X\r\n", r1);
        return _abort_transaction(hsd, SD_ERR_NOT_RESPONDING);
    }

    /* 4. ACMD41 — Loop de inicialização */
    uint32_t arg_acmd41 = is_v2 ? 0x40000000 : 0x00000000;
    uint32_t retry = 0;
    printf("Iniciando loop ACMD41...\r\n");
    do
    {
        _send_cmd(SD_CMD55, 0x00000000, 0xFF);
        r1 = _send_cmd(SD_ACMD41, arg_acmd41, 0xFF);
        HAL_Delay(1);
    } while ((r1 != R1_READY) && (++retry < INIT_RETRY_MAX));

    printf("ACMD41 finalizado. R1: 0x%02X\r\n", r1);

    if (r1 != R1_READY)
    {
        return _abort_transaction(hsd, SD_ERR_TIMEOUT);
    }

    /* 5. CMD58 — Lê OCR */
    if (is_v2)
    {
        r1 = _send_cmd(SD_CMD58, 0x00000000, 0xFF);
        if (r1 != R1_READY)
        {
            printf("[SD] Falha ao ler OCR (CMD58): 0x%02X\r\n", r1);
            return _abort_transaction(hsd, SD_ERR_IO);
        }
        uint8_t ocr[4];
        for (int i = 0; i < 4; i++)
        {
            ocr[i] = SPI1_SendByte(0xFF);
        }
        hsd->type = (ocr[0] & 0x40u) ? SD_TYPE_SDHC : SD_TYPE_V2;
    }
    else
    {
        hsd->type = SD_TYPE_V1;
    }

    if (hsd->type != SD_TYPE_SDHC)
    {
        r1 = _send_cmd(SD_CMD16, 512u, 0xFFu);
        if (r1 != R1_READY)
        {
            printf("[SD] Falha ao configurar blocos de 512 bytes.\r\n");
            return _abort_transaction(hsd, SD_ERR_INIT);
        }
    }

    SD_Status_t csd_status = _read_csd(hsd);
    if (csd_status != SD_OK)
    {
        printf("[SD] Falha ao ler CSD: %s\r\n", SD_StatusStr(csd_status));
        return _abort_transaction(hsd, csd_status);
    }

    /* 6. Aumenta a velocidade somente após inicialização completa. */
    SD_Deselect();
    if (SPI1_SetSpeed(SPI1_SPEED_FAST) != HAL_OK)
    {
        return SD_ERR_INIT;
    }

    hsd->initialized = true;
    printf("[SD] Inicializado: tipo=%u, setores=%lu\r\n", (unsigned)hsd->type,
           (unsigned long)hsd->sector_count);
    return SD_OK;
}

SD_Status_t SD_SPI_ReadBlock(SD_Handle_t *hsd, uint32_t sector, uint8_t *buffer)
{
    if ((hsd == NULL) || (buffer == NULL))
    {
        return SD_ERR_PARAM;
    }
    if (!hsd->initialized)
    {
        return SD_ERR_NOT_RESPONDING;
    }
    if ((hsd->sector_count > 0u) && (sector >= hsd->sector_count))
    {
        return SD_ERR_PARAM;
    }

    /*
     * SDHC/SDXC: endereça por número de bloco (sector).
     * SDSC v1/v2: endereça por byte → multiplica por 512.
     */
    uint32_t addr = (hsd->type == SD_TYPE_SDHC) ? sector : sector * 512;

    SD_Select();

    /* Envia CMD17 e aguarda R1 = 0x00 */
    uint8_t r1 = _send_cmd(SD_CMD17, addr, 0xFF);
    if (r1 != R1_READY)
    {
        return _abort_transaction(hsd, SD_ERR_IO);
    }

    /* Aguarda o token de início de dados (0xFE) */
    if (!_wait_start_token())
    {
        return _abort_transaction(hsd, SD_ERR_TIMEOUT);
    }

    /* Transfere os 512 bytes via DMA */
    HAL_StatusTypeDef dma_ret = SPI1_DMA_ReadBlock(buffer);

    /* Consome os 2 bytes de CRC (descartados — CRC desabilitado após CMD0) */
    SPI1_SendByte(0xFF);
    SPI1_SendByte(0xFF);

    SD_Deselect();
    /* 1 byte extra de clock após deselect (boa prática SPI) */
    SPI1_SendByte(0xFF);

    if (dma_ret != HAL_OK)
    {
        hsd->initialized = false;
        return SD_ERR_IO;
    }
    return SD_OK;
}

SD_Status_t SD_SPI_WriteBlock(SD_Handle_t *hsd, uint32_t sector, const uint8_t *buffer)
{
    if ((hsd == NULL) || (buffer == NULL))
    {
        return SD_ERR_PARAM;
    }
    if (!hsd->initialized)
    {
        return SD_ERR_NOT_RESPONDING;
    }
    if ((hsd->sector_count > 0u) && (sector >= hsd->sector_count))
    {
        return SD_ERR_PARAM;
    }

    uint32_t addr = (hsd->type == SD_TYPE_SDHC) ? sector : sector * 512;

    SD_Select();

    /* Envia CMD24 e aguarda R1 = 0x00 */
    uint8_t r1 = _send_cmd(SD_CMD24, addr, 0xFF);
    if (r1 != R1_READY)
    {
        return _abort_transaction(hsd, SD_ERR_IO);
    }

    /* 1 byte dummy + token de início de bloco */
    SPI1_SendByte(0xFF);
    SPI1_SendByte(TOKEN_START_BLOCK);

    /* Transfere os 512 bytes de dados via DMA */
    HAL_StatusTypeDef dma_ret = SPI1_DMA_WriteBlock(buffer);
    if (dma_ret != HAL_OK)
    {
        return _abort_transaction(hsd, SD_ERR_IO);
    }

    /* 2 bytes de CRC dummy (0xFF 0xFF — CRC desabilitado) */
    SPI1_SendByte(0xFF);
    SPI1_SendByte(0xFF);

    /*
     * Lê o token de resposta de dados.
     * Os bits [4:0] indicam o resultado:
     *   0b00101 (0x05) = dados aceitos.
     *   0b01011        = CRC error.
     *   0b01101        = Write error.
     */
    uint8_t resp = SPI1_SendByte(0xFF) & DATA_RESP_MASK;
    if (resp != DATA_RESP_OK)
    {
        return _abort_transaction(hsd, SD_ERR_IO);
    }

    /*
     * Aguarda o cartão terminar a programação interna do flash.
     * Durante esse tempo, MISO fica em nível baixo (busy token = 0x00).
     * PITFALL: não deselecionar o CS durante o busy wait — isso abortaria
     * a escrita e poderia corromper o setor.
     */
    if (!_wait_not_busy(BUSY_WAIT_MAX))
    {
        return _abort_transaction(hsd, SD_ERR_TIMEOUT);
    }

    SD_Deselect();
    SPI1_SendByte(0xFF);

    return SD_OK;
}

SD_Status_t SD_SPI_Sync(SD_Handle_t *hsd)
{
    if ((hsd == NULL) || !hsd->initialized)
    {
        return SD_ERR_NOT_RESPONDING;
    }

    SD_Select();
    bool ready = _wait_not_busy(BUSY_WAIT_MAX);
    SD_Deselect();
    (void)SPI1_SendByte(0xFFu);

    if (!ready)
    {
        hsd->initialized = false;
        return SD_ERR_TIMEOUT;
    }
    return SD_OK;
}

const char *SD_StatusStr(SD_Status_t status)
{
    switch (status)
    {
        case SD_OK:
            return "OK";
        case SD_ERR_NOT_RESPONDING:
            return "CARTAO/SPI NAO RESPONDEU";
        case SD_ERR_TIMEOUT:
            return "TIMEOUT - Cartao nao respondeu no prazo";
        case SD_ERR_INIT:
            return "FALHA DE INIT - Cartao incompativel ou danificado";
        case SD_ERR_IO:
            return "ERRO DE I/O - Falha na leitura ou escrita do bloco";
        case SD_ERR_PARAM:
            return "PARAMETRO INVALIDO";
        default:
            return "ERRO DESCONHECIDO";
    }
}
