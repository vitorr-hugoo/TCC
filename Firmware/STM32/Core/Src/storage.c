/**
 ******************************************************************************
 * @file    storage.c
 * @brief   Implementação da API de armazenamento da estação meteorológica
 ******************************************************************************
 */

#include "storage.h"
#include "sd_card.h"
#include "sd_spi.h"
#include <stdio.h>
#include <string.h>

/* ─── Estado interno ─────────────────────────────────────────────────────── */
static FATFS  s_fatfs;                /**< Objeto de volume FatFs            */
static FIL    s_file;                 /**< Handle do arquivo data.csv        */
static bool   s_initialized = false; /**< true após Storage_Init bem-sucedida */
static bool   s_file_open = false;
static bool   s_dma_initialized = false;
static uint32_t s_sample_count = 0;  /**< Contador de amostras desde último sync */
static uint32_t s_next_retry_ms = 0;
static StorageState_t s_state = STORAGE_OFFLINE;
static FRESULT s_last_error = FR_NOT_READY;

/* Cabeçalho CSV — deve coincidir com os campos de SensorSample_t */
static const char *CSV_HEADER =
    "timestamp_ms;temp_c;humidity_pct;pressure_hpa;"
    "irrad_wm2;wind_speed_ms;wind_dir_deg;rain_mm\r\n";

/* ─── Helpers internos ───────────────────────────────────────────────────── */

/**
 * @brief  Cria um diretório se ainda não existir.
 *         f_mkdir retorna FR_EXIST se o diretório já existe — isso não é erro.
 */
static FRESULT _ensure_dir(const char *path)
{
    FRESULT res = f_mkdir(path);
    return (res == FR_EXIST) ? FR_OK : res;
}

/**
 * @brief  Abre data.csv em modo append. Se não existir, cria e grava cabeçalho.
 */
static FRESULT _open_log_file(void)
{
    /* FA_OPEN_APPEND: abre existente e posiciona no final; cria se não existir */
    FRESULT res = f_open(&s_file, STORAGE_LOG_FILE,
                          FA_WRITE | FA_READ | FA_OPEN_APPEND);
    if (res != FR_OK) return res;
    s_file_open = true;

    /*
     * Se o arquivo acabou de ser criado, seu tamanho é zero.
     * Grava o cabeçalho CSV apenas nesse caso.
     */
    if (f_size(&s_file) == 0) {
        UINT bw;
        res = f_write(&s_file, CSV_HEADER, strlen(CSV_HEADER), &bw);
        if ((res != FR_OK) || (bw != strlen(CSV_HEADER)))
            return (res != FR_OK) ? res : FR_DISK_ERR;
        res = f_sync(&s_file);
        if (res != FR_OK) return res;
    }
    return FR_OK;
}

static void _enter_fault(FRESULT reason)
{
    if (s_file_open)
    {
        (void)f_close(&s_file);
        s_file_open = false;
    }

    (void)f_mount(NULL, "0:", 0);
    SD_Deselect();

    s_initialized = false;
    s_state = STORAGE_FAULT;
    s_last_error = reason;
    s_next_retry_ms = HAL_GetTick() + STORAGE_RETRY_INTERVAL_MS;
}

/* ─── API pública ────────────────────────────────────────────────────────── */

bool Storage_Init(void)
{
    if (s_state == STORAGE_READY)
    {
        return true;
    }

    s_state = STORAGE_INITIALIZING;
    s_initialized  = false;
    s_sample_count = 0;

    if (!s_dma_initialized)
    {
        SPI1_DMA_Init();
        s_dma_initialized = true;
    }

    /* ── 2. Inicializa o cartão SD ──────────────────────────────────── */
    /*
     * sd_handle é gerenciado internamente pelo user_diskio.c via USER_initialize.
     * Aqui chamamos f_mount que invoca USER_initialize automaticamente.
     */

    /* ── 3. Monta o volume FatFs (drive "0:") ───────────────────────── */
    /*
     * O segundo argumento '1' força a montagem imediata (tenta acessar o
     * volume agora) em vez de lazy (monta na primeira operação de arquivo).
     * Isso detecta falhas de cartão cedo, na inicialização.
     */
    FRESULT res = f_mount(&s_fatfs, "0:", 1);
    if (res != FR_OK)
    {
        printf("[STORAGE] Falha ao montar volume: %d\r\n", (int)res);
        _enter_fault(res);
        return false;
    }

    /* ── 4. Garante que o diretório /logs existe ────────────────────── */
    res = _ensure_dir(STORAGE_LOG_DIR);
    if (res != FR_OK) {
        printf("[STORAGE] Falha ao criar diretorio: %d\r\n", (int)res);
        _enter_fault(res);
        return false;
    }

    /* ── 5. Abre (ou cria) o arquivo de log ─────────────────────────── */
    res = _open_log_file();
    if (res != FR_OK) {
        printf("[STORAGE] Falha ao abrir arquivo: %d\r\n", (int)res);
        _enter_fault(res);
        return false;
    }

    s_initialized = true;
    s_state = STORAGE_READY;
    s_last_error = FR_OK;
    printf("[STORAGE] Volume pronto em %s\r\n", STORAGE_LOG_FILE);
    return true;
}

void Storage_Service(uint32_t now_ms)
{
    if ((s_state == STORAGE_READY) || (s_state == STORAGE_INITIALIZING))
    {
        return;
    }

    if ((int32_t)(now_ms - s_next_retry_ms) >= 0)
    {
        printf("[STORAGE] Tentando recuperar o cartao...\r\n");
        (void)Storage_Init();
    }
}

StorageState_t Storage_GetState(void)
{
    return s_state;
}

bool Storage_IsReady(void)
{
    return (s_state == STORAGE_READY) && s_initialized && s_file_open;
}

FRESULT Storage_GetLastError(void)
{
    return s_last_error;
}

FRESULT Storage_AppendSample(const SensorSample_t *sample)
{
    if (sample == NULL) return FR_INVALID_PARAMETER;
    if (!Storage_IsReady()) return FR_NOT_READY;

    /* ── 1. Verifica espaço livre antes de gravar ───────────────────── */
    uint32_t free_kb = 0;
    FRESULT res = Storage_CheckCardSpace(NULL, &free_kb);
    if (res != FR_OK)
    {
        _enter_fault(res);
        return res;
    }

    // Se tiver menos de 5MB livres, bloqueia a gravação para não corromper o SD
    if (free_kb < STORAGE_MIN_FREE_KB) {
        return FR_DENIED;
    }

    /* ── 2. Formata a linha CSV (CORRIGIDO PARA PONTO E VÍRGULA) ────── */
    char line[128];
    int  len = snprintf(line, sizeof(line),
        "%lu;%.1f;%.1f;%.2f;%.2f;%.2f;%.1f;%.2f\r\n", // <-- Trocado de ',' para ';'
        (unsigned long)sample->timestamp_ms,
        sample->temperature,
        sample->humidity,
        sample->pressure,
        sample->irradiance,
        sample->wind_speed,
        sample->wind_dir_deg,
        sample->rain_mm);

    if (len <= 0 || len >= (int)sizeof(line)) return FR_INVALID_PARAMETER;

    /* ── 3. Grava no arquivo ─────────────────────────────────────────── */
    UINT bw;
    res = f_write(&s_file, line, (UINT)len, &bw);
    if (res != FR_OK || bw != (UINT)len)
    {
        FRESULT write_error = (res != FR_OK) ? res : FR_DISK_ERR;
        _enter_fault(write_error);
        return write_error;
    }

    /* ── 4. Sync periódico para reduzir latência e desgaste ─────────── */
        s_sample_count++;

        if (s_sample_count >= STORAGE_SYNC_INTERVAL)
        {
            res = f_sync(&s_file);
            if (res != FR_OK)
            {
                _enter_fault(res);
                return res;
            }

            s_sample_count = 0;
        }

        return FR_OK;
    }
FRESULT Storage_CheckCardSpace(uint32_t *used_kb, uint32_t *free_kb)
{
    if (!Storage_IsReady()) return FR_NOT_READY;

    FATFS   *fs_ptr;
    DWORD    free_clusters;

    FRESULT res = f_getfree("0:", &free_clusters, &fs_ptr);
    if (res != FR_OK) return res;

    /* Cada cluster tem fs_ptr->csize setores de 512 bytes */
    uint32_t total_kb = (uint32_t)((fs_ptr->n_fatent - 2UL)
                         * fs_ptr->csize / 2UL);   /* /2 = /1024 * 512 */
    uint32_t free_kb_val = (uint32_t)(free_clusters
                            * fs_ptr->csize / 2UL);

    if (free_kb)  *free_kb  = free_kb_val;
    if (used_kb)  *used_kb  = total_kb - free_kb_val;

    return FR_OK;
}

void Storage_Deinit(void)
{
    if (s_file_open)
    {
        (void)f_sync(&s_file);
        (void)f_close(&s_file);
        s_file_open = false;
    }
    (void)f_mount(NULL, "0:", 0);  /* Desmonta o volume */
    SD_Deselect();
    s_initialized = false;
    s_state = STORAGE_OFFLINE;
    s_last_error = FR_NOT_READY;
}

/* Separate append-only diagnostics; data.csv keeps its existing schema. */
FRESULT Storage_AppendSensorHealth(uint32_t now, SensorId sensor,
                                  const SensorHealth *health, HealthEvent event)
{
    if (!health || (unsigned)sensor >= SENSOR_COUNT) return FR_INVALID_PARAMETER;
    if (!Storage_IsReady()) return FR_NOT_READY;
    uint32_t free_kb;
    FRESULT res = Storage_CheckCardSpace(NULL, &free_kb);
    if (res != FR_OK) { _enter_fault(res); return res; }
    if (free_kb < STORAGE_MIN_FREE_KB) return FR_DENIED;
    /* Storage is serviced only by main. Keep FatFs' sector buffer out of
     * the call stack, as already done for data.csv. */
    static FIL file;
    res = f_open(&file, "0:/logs/sensor_health.csv", FA_WRITE | FA_OPEN_APPEND);
    if (res != FR_OK) { _enter_fault(res); return res; }
    UINT written;
    if (f_size(&file) == 0) {
        const char *header = "timestamp_ms;sensor;evento;valido;motivo;falhas;tem_ultima_valida;ultima_valida_ms;idade_ms\r\n";
        res = f_write(&file, header, (UINT)strlen(header), &written);
        if (res == FR_OK && written != strlen(header)) res = FR_DISK_ERR;
    }
    if (res == FR_OK) {
        char line[180];
        int len = snprintf(line, sizeof(line), "%lu;%s;%s;%u;%s;%u;%u;%lu;%lu\r\n",
            (unsigned long)now, Sensor_Name(sensor), Sensor_Event(event), health->valid,
            Sensor_Reason(health->reason), health->consecutive_failures, health->has_valid,
            (unsigned long)health->last_valid_ms,
            (unsigned long)(health->has_valid ? now - health->last_valid_ms : 0));
        if (len <= 0 || len >= (int)sizeof(line)) res = FR_INVALID_PARAMETER;
        else {
            res = f_write(&file, line, (UINT)len, &written);
            if (res == FR_OK && written != (UINT)len) res = FR_DISK_ERR;
        }
    }
    /* close flushes this event; preserve the first error. */
    FRESULT close_res = f_close(&file);
    if (res == FR_OK) res = close_res;
    if (res != FR_OK) _enter_fault(res);
    return res;
}
