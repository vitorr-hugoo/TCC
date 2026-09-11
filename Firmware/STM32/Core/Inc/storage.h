/**
 ******************************************************************************
 * @file    storage.h
 * @brief   Camada 4 — API de armazenamento para a estação meteorológica
 *
 * Esta é a única camada que o restante da aplicação deve conhecer.
 * Sensores, LoRa e máquina de estados principal chamam apenas Storage_*;
 * nunca acessam FatFs, sd_card ou sd_spi diretamente.
 *
 * Estrutura de arquivos no cartão SD:
 *   /logs/
 *       data.csv      ← arquivo principal de log
 *
 * Formato CSV (cabeçalho na primeira linha):
 *   timestamp_ms,temp_c,humidity_pct,pressure_hpa,irrad_wm2,
 *   wind_speed_ms,wind_dir_deg,rain_mm
 ******************************************************************************
 */

#ifndef STORAGE_H
#define STORAGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ff.h"       /* FatFs API */
#include <stdint.h>
#include <stdbool.h>

/* ─── Configurações ──────────────────────────────────────────────────────── */

/** Caminho do diretório e arquivo de log */
#define STORAGE_LOG_DIR   "0:/logs"
#define STORAGE_LOG_FILE  "0:/logs/data.csv"

/**
 * Número de amostras entre chamadas de f_sync.
 * Valores menores: mais seguro contra perda de dados em queda de energia.
 * Valores maiores: menor desgaste do cartão por operações de flush.
 * Recomendação: 10 amostras × 60 s = sync a cada 10 min.
 */
#define STORAGE_SYNC_INTERVAL  10u

/** Intervalo entre tentativas de recuperar um cartão indisponível. */
#define STORAGE_RETRY_INTERVAL_MS  10000u

/**
 * Espaço livre mínimo em kB antes de parar de gravar.
 * Abaixo desse valor, Storage_AppendSample retorna FR_DENIED e acende
 * o LED de aviso.
 */
#define STORAGE_MIN_FREE_KB    1024u  /**< 1 MB de reserva mínima */

/* ─── Estrutura de uma amostra completa ──────────────────────────────────── */
typedef struct {
    uint32_t timestamp_ms;   /**< HAL_GetTick() no momento da leitura    */
    float    temperature;    /**< °C  (DHT22 ou BMP280)                  */
    float    humidity;       /**< %   (DHT22)                            */
    float    pressure;       /**< hPa (BMP280)                           */
    float    irradiance;     /**< W/m² (BH1750, estimado)                */
    float    wind_speed;     /**< m/s  (anemômetro)                      */
    float    wind_dir_deg;   /**< graus (biruta)                         */
    float    rain_mm;        /**< mm acumulado (pluviômetro)             */
} SensorSample_t;

typedef enum {
    STORAGE_OFFLINE = 0,
    STORAGE_INITIALIZING,
    STORAGE_READY,
    STORAGE_FAULT
} StorageState_t;

/* ─── Protótipos ─────────────────────────────────────────────────────────── */

/**
 * @brief  Inicializa toda a pilha de armazenamento:
 *           1. SPI1_DMA_Init → preenche buffer DMA.
 *           2. SD_SPI_InitCard → detecta e inicializa o cartão.
 *           3. f_mount → monta o volume FatFs.
 *           4. Cria /logs e data.csv se não existirem.
 *           5. Abre data.csv para escrita (modo append).
 *
 * @note   Chamar uma vez no main() após MX_FATFS_Init().
 *         Se retornar false, as demais funções retornam FR_NOT_ENABLED
 *         sem tentar acessar o cartão.
 *
 * @retval true se pronto para gravar, false em caso de falha.
 */
bool Storage_Init(void);

/** Executa tentativas periódicas de recuperação sem bloquear a aquisição. */
void Storage_Service(uint32_t now_ms);

/** Retorna o estado atual da camada de armazenamento. */
StorageState_t Storage_GetState(void);

/** Atalho para verificar se o arquivo está montado e pronto para escrita. */
bool Storage_IsReady(void);

/** Retorna o último erro informado pelo FatFs. */
FRESULT Storage_GetLastError(void);

/**
 * @brief  Grava uma linha CSV com os dados de uma amostra.
 *
 * Comportamento:
 *   - Converte SensorSample_t em string CSV e chama f_write.
 *   - A cada STORAGE_SYNC_INTERVAL amostras, chama f_sync.
 *   - Antes de gravar, verifica espaço livre com Storage_CheckCardSpace.
 *     Se abaixo de STORAGE_MIN_FREE_KB, para de gravar e retorna FR_DENIED.
 *
 * @param  sample  Ponteiro para a amostra a ser gravada.
 * @retval FR_OK | FR_DENIED (sem espaço) | FR_NOT_ENABLED (não init) | ...
 */
FRESULT Storage_AppendSample(const SensorSample_t *sample);

/**
 * @brief  Consulta o espaço usado e livre no cartão via f_getfree.
 *
 * @param  used_kb  Ponteiro para receber kB usados (pode ser NULL).
 * @param  free_kb  Ponteiro para receber kB livres (pode ser NULL).
 * @retval FR_OK ou código de erro FatFs.
 */
FRESULT Storage_CheckCardSpace(uint32_t *used_kb, uint32_t *free_kb);

/**
 * @brief  Fecha o arquivo de log e desmonta o volume.
 *         Chamar antes de um reset ou desligamento planejado.
 */
void Storage_Deinit(void);


#ifdef __cplusplus
}
#endif
#endif /* STORAGE_H */
