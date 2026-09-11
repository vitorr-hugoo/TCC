/**
 ******************************************************************************
 * @file    bmp280.h
 * @brief   Driver para o sensor de pressão e temperatura BMP280 (Bosch Sensortec)
 *          Interface I²C — Compatível com STM32 HAL (série F4).
 *
 * @details O BMP280 armazena na sua memória não-volátil (NVM) 12 coeficientes de
 *          calibração individuais (dig_T1..T3, dig_P1..P9). Sem eles, os dados
 *          ADC brutos são sem sentido. Esta biblioteca:
 *            1. Verifica o chip ID (deve ser 0x60 para o BMP280).
 *            2. Lê e armazena todos os coeficientes na struct de handle.
 *            3. Aplica as fórmulas de compensação inteiras de 64 bits do
 *               datasheet da Bosch (Seção 4.2.3), garantindo a precisão
 *               especificada de ±0,12 hPa.
 *            4. Calcula altitude relativa a partir da pressão e da pressão
 *               ao nível do mar configurável.
 *
 * Endereço I²C:
 *   SDO = GND → 0x76  (padrão dos módulos GY-BMP280)
 *   SDO = VCC → 0x77
 *
 * Como usar:
 *   1. Configure e inicialize o periférico I2C no CubeMX (I2C1 ou I2C2).
 *   2. Declare o handle e a config:
 *        BMP280_Handle_t bmp;
 *        BMP280_Config_t cfg = BMP280_DEFAULT_CONFIG;
 *   3. Inicialize (uma vez, no main após MX_I2Cx_Init):
 *        BMP280_Status_t st = BMP280_Init(&bmp, &hi2c1, BMP280_ADDR_SDO_LOW, &cfg);
 *   4. No loop, leia:
 *        st = BMP280_Read(&bmp);
 *        if (st == BMP280_OK) { float p = bmp.pressure; }
 ******************************************************************************
 */

#ifndef BMP280_H
#define BMP280_H

#ifdef __cplusplus
extern "C" {
#endif

/* ─── Includes ───────────────────────────────────────────────────────────── */
#include "stm32f4xx_hal.h"
#include <stdint.h>

/* ─── Endereços I²C (formato 7 bits — HAL faz o shift internamente) ──────── */
#define BMP280_ADDR_SDO_LOW    0x76u   /**< SDO ligado ao GND (módulo GY-BMP280 padrão) */
#define BMP280_ADDR_SDO_HIGH   0x77u   /**< SDO ligado ao VCC                           */

/* ─── Identificador do chip ─────────────────────────────────────────────── */
#define BMP280_CHIP_ID         0x58u   /**< Valor esperado no registrador 0xD0           */

/* ─── Timeout I²C (ms) ──────────────────────────────────────────────────── */
#define BMP280_I2C_TIMEOUT     100u

/* ─── Pressão padrão ao nível do mar (hPa) ─────────────────────────────── */
#define BMP280_SEA_LEVEL_HPA   1013.25f

/* ─── Enumerações de configuração ───────────────────────────────────────── */

/** @brief Fator de oversampling para temperatura e pressão */
typedef enum {
    BMP280_OS_SKIPPED = 0x00, /**< Medição desabilitada           */
    BMP280_OS_1X      = 0x01, /**< Oversampling x1                */
    BMP280_OS_2X      = 0x02, /**< Oversampling x2                */
    BMP280_OS_4X      = 0x03, /**< Oversampling x4                */
    BMP280_OS_8X      = 0x04, /**< Oversampling x8                */
    BMP280_OS_16X     = 0x05, /**< Oversampling x16 (máx. precis.)*/
} BMP280_Oversampling_t;

/** @brief Coeficiente do filtro IIR interno */
typedef enum {
    BMP280_FILTER_OFF = 0x00, /**< Sem filtragem                  */
    BMP280_FILTER_2   = 0x01,
    BMP280_FILTER_4   = 0x02,
    BMP280_FILTER_8   = 0x03,
    BMP280_FILTER_16  = 0x04, /**< Máximo amortecimento de picos  */
} BMP280_Filter_t;

/** @brief Tempo de standby em modo Normal (entre medições automáticas) */
typedef enum {
    BMP280_STANDBY_0_5MS  = 0x00,
    BMP280_STANDBY_62_5MS = 0x01,
    BMP280_STANDBY_125MS  = 0x02,
    BMP280_STANDBY_250MS  = 0x03,
    BMP280_STANDBY_500MS  = 0x04,
    BMP280_STANDBY_1000MS = 0x05,
    BMP280_STANDBY_2000MS = 0x06,
    BMP280_STANDBY_4000MS = 0x07,
} BMP280_Standby_t;

/** @brief Modo de operação do sensor */
typedef enum {
    BMP280_MODE_SLEEP  = 0x00, /**< Sem medição, consumo mínimo   */
    BMP280_MODE_FORCED = 0x01, /**< Uma medição, volta para Sleep  */
    BMP280_MODE_NORMAL = 0x03, /**< Medição contínua automática    */
} BMP280_Mode_t;

/* ─── Status de retorno ─────────────────────────────────────────────────── */
typedef enum {
    BMP280_OK              = 0x00, /**< Operação bem-sucedida          */
    BMP280_ERR_NOT_FOUND   = 0x01, /**< Chip ID incorreto / sem sensor */
    BMP280_ERR_I2C         = 0x02, /**< Falha na comunicação I²C       */
    BMP280_ERR_BUSY        = 0x03, /**< Sensor ainda medindo           */
    BMP280_ERR_CONFIG = 4,
    BMP280_ERR_CALIBRATION = 5,
    BMP280_ERR_RANGE = 6,
} BMP280_Status_t;

/* ─── Struct de configuração ────────────────────────────────────────────── */
typedef struct {
    BMP280_Oversampling_t os_temperature; /**< Oversampling da temperatura   */
    BMP280_Oversampling_t os_pressure;    /**< Oversampling da pressão       */
    BMP280_Filter_t       filter;         /**< Coeficiente do filtro IIR     */
    BMP280_Standby_t      standby;        /**< Standby (modo Normal)         */
    BMP280_Mode_t         mode;           /**< Modo de operação              */
    float                 sea_level_hpa;  /**< Pressão ao nível do mar (hPa) */
} BMP280_Config_t;

/**
 * @brief Configuração padrão recomendada para estação meteorológica.
 *
 * Oversampling pressão x16 + temperatura x2 + filtro IIR 16 → precisão
 * relativa de ±0,12 hPa conforme datasheet Bosch.
 * Modo Normal com standby 1000 ms: leitura autônoma a cada ~1 s.
 */
#define BMP280_DEFAULT_CONFIG  {            \
    .os_temperature = BMP280_OS_2X,         \
    .os_pressure    = BMP280_OS_16X,        \
    .filter         = BMP280_FILTER_16,     \
    .standby        = BMP280_STANDBY_1000MS,\
    .mode           = BMP280_MODE_NORMAL,   \
    .sea_level_hpa  = BMP280_SEA_LEVEL_HPA  \
}

/* ─── Handle do sensor ─────────────────────────────────────────────────── */
typedef struct {

    /* ── Recursos de hardware (preenchidos por BMP280_Init) ── */
    I2C_HandleTypeDef *hi2c;           /**< Handle do barramento I²C        */
    uint16_t           dev_addr;       /**< Endereço I²C deslocado (<<1)    */
    float              sea_level_hpa;  /**< Pressão ao nível do mar (hPa)   */

    /* ── Coeficientes de calibração (lidos da NVM na inicialização) ── */
    uint16_t dig_T1;
    int16_t  dig_T2, dig_T3;
    uint16_t dig_P1;
    int16_t  dig_P2, dig_P3, dig_P4, dig_P5,
             dig_P6, dig_P7, dig_P8, dig_P9;

    /* ── Variável compartilhada entre compensação de T e P ── */
    int32_t  t_fine;

    /* ── Dados lidos (válidos após BMP280_Read retornar BMP280_OK) ── */
    float temperature; /**< Temperatura em °C                              */
    float pressure;    /**< Pressão atmosférica em hPa                     */
    float altitude;    /**< Altitude relativa em metros (fórmula barométrica)*/

    uint8_t initialized, expected_ctrl, expected_config;
    uint32_t measurement_timeout_ms;
} BMP280_Handle_t;

/* ─── Protótipos ────────────────────────────────────────────────────────── */

/**
 * @brief  Inicializa o sensor: verifica chip ID, lê calibração e configura
 *         registradores ctrl_meas e config.
 *
 * @param  hdev      Ponteiro para o handle.
 * @param  hi2c      Handle do barramento I²C já inicializado pelo HAL.
 * @param  addr_7bit Endereço I²C de 7 bits (BMP280_ADDR_SDO_LOW ou _HIGH).
 * @param  cfg       Ponteiro para a struct de configuração (use &cfg ou
 *                   ponteiro para BMP280_DEFAULT_CONFIG).
 * @retval BMP280_OK | BMP280_ERR_NOT_FOUND | BMP280_ERR_I2C
 */
BMP280_Status_t BMP280_Init(BMP280_Handle_t      *hdev,
                            I2C_HandleTypeDef    *hi2c,
                            uint8_t               addr_7bit,
                            const BMP280_Config_t *cfg);

/**
 * @brief  Lê temperatura, pressão e calcula altitude.
 *
 * @note   Em modo Normal, a leitura ocorre imediatamente (sensor mede
 *         continuamente). Em modo Forced, dispara uma medição e aguarda
 *         a conclusão (status busy) antes de ler.
 *
 * @param  hdev  Ponteiro para o handle inicializado.
 * @retval BMP280_OK | BMP280_ERR_I2C | BMP280_ERR_BUSY
 */
BMP280_Status_t BMP280_Read(BMP280_Handle_t *hdev);

/**
 * @brief  Envia um soft-reset ao sensor (equivale a power-on reset).
 *         Após o reset, chame BMP280_Init novamente.
 *
 * @param  hdev  Ponteiro para o handle.
 * @retval BMP280_OK | BMP280_ERR_I2C
 */
BMP280_Status_t BMP280_Reset(BMP280_Handle_t *hdev);

/**
 * @brief  Verifica se o sensor está respondendo no barramento I²C.
 *
 * @param  hdev  Ponteiro para o handle.
 * @retval BMP280_OK (respondeu) | BMP280_ERR_NOT_FOUND (sem resposta)
 */
BMP280_Status_t BMP280_IsConnected(BMP280_Handle_t *hdev);

/**
 * @brief  Retorna string descritiva do código de status.
 * @param  status  Código retornado pelas funções da lib.
 * @retval Ponteiro para string estática (não modificar).
 */
const char *BMP280_StatusStr(BMP280_Status_t status);


#ifdef __cplusplus
}
#endif

#endif /* BMP280_H */
