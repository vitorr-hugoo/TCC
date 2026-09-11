/**
 ******************************************************************************
 * @file    bh1750.h
 * @brief   Driver para o sensor de iluminância BH1750 (ROHM Semiconductor)
 *          Interface I²C — Compatível com STM32 HAL (série F4).
 *
 * @details O BH1750 difere do BMP280 em um ponto fundamental no protocolo I²C:
 *          ele NÃO possui registradores internos endereçáveis. A comunicação
 *          funciona apenas com comandos diretos (1 byte TX) e leitura direta
 *          (2 bytes RX), sem registro de destino. Por isso esta biblioteca usa
 *          HAL_I2C_Master_Transmit / HAL_I2C_Master_Receive — e NÃO
 *          HAL_I2C_Mem_Read, que exige um endereço de registrador.
 *
 *          Além da leitura em lux, a biblioteca estima a irradiância solar
 *          global em W/m² usando o fator de conversão do Guia CIMO N°8 da OMM
 *          (≈ 1/126,7 W·m⁻²·lux⁻¹ para luz solar direta). Um fator de
 *          calibração ajustável permite refinar esse valor após comparação com
 *          a estação de referência da UFTM (requisito RF-11 do projeto).
 *
 * Endereço I²C (pino ADDR do módulo GY-30):
 *   ADDR = GND → 0x23  (padrão da maioria dos módulos)
 *   ADDR = VCC → 0x5C
 *
 * Como usar:
 *   1. Configure I2C no CubeMX (Fast Mode, 400 kHz recomendado).
 *   2. Declare o handle:
 *        BH1750_Handle_t bh;
 *   3. Inicialize:
 *        BH1750_Status_t st = BH1750_Init(&bh, &hi2c1,
 *                                         BH1750_ADDR_LOW,
 *                                         BH1750_MODE_CONT_H_RES);
 *   4. Aguarde ao menos 180 ms após Init (tempo de primeira medição).
 *   5. Leia no loop:
 *        st = BH1750_Read(&bh);
 *        if (st == BH1750_OK) { float lux = bh.illuminance; }
 ******************************************************************************
 */

#ifndef BH1750_H
#define BH1750_H

#ifdef __cplusplus
extern "C" {
#endif

/* ─── Includes ───────────────────────────────────────────────────────────── */
#include "stm32f4xx_hal.h"
#include <stdint.h>

/* ─── Endereços I²C (7 bits) ─────────────────────────────────────────────── */
#define BH1750_ADDR_LOW    0x23u  /**< ADDR = GND (módulo GY-30 padrão) */
#define BH1750_ADDR_HIGH   0x5Cu  /**< ADDR = VCC                        */

/* ─── Timeout I²C (ms) ──────────────────────────────────────────────────── */
#define BH1750_I2C_TIMEOUT  100u

/* ─── Fator de conversão lux → W/m² (OMM CIMO Guide No.8) ──────────────── */
/*
 * Para luz solar direta: 1 W/m² ≈ 126,7 lux.
 * Este valor é o ponto de partida antes da calibração experimental
 * com a estação de referência da UFTM (RF-11).
 * Use BH1750_SetCalibration() para ajustar após os ensaios.
 */
#define BH1750_DEFAULT_K_FACTOR   (1.0f / 126.7f)  /**< W·m⁻²·lux⁻¹ */

/* ─── Comandos do sensor (enviados diretamente via I²C) ─────────────────── */
#define BH1750_CMD_POWER_DOWN    0x00u  /**< Desliga o sensor                  */
#define BH1750_CMD_POWER_ON      0x01u  /**< Liga o sensor (estado de espera)  */
#define BH1750_CMD_RESET         0x07u  /**< Zera o registrador de dados       */

/* ─── Modos de medição ───────────────────────────────────────────────────── */
/**
 * @brief Modos de operação do BH1750.
 *
 * H-Resolution  → resolução 1,0 lux   | tempo de medição ~120 ms
 * H-Resolution2 → resolução 0,5 lux   | tempo de medição ~120 ms (mais preciso)
 * L-Resolution  → resolução 4,0 lux   | tempo de medição ~16 ms  (mais rápido)
 *
 * Modos Continuous: sensor mede repetidamente de forma autônoma.
 *   BH1750_Read lê o valor do último ciclo concluído.
 *
 * Modos One Time: sensor mede uma vez e entra em Power Down.
 *   Necessário reenviar o comando a cada leitura desejada.
 *   Útil para economizar energia nos ciclos de sleep do STM32.
 */
typedef enum {
    BH1750_MODE_CONT_H_RES    = 0x10u, /**< Contínuo, 1,0 lux  — RECOMENDADO */
    BH1750_MODE_CONT_H_RES2   = 0x11u, /**< Contínuo, 0,5 lux                 */
    BH1750_MODE_CONT_L_RES    = 0x13u, /**< Contínuo, 4,0 lux                 */
    BH1750_MODE_ONCE_H_RES    = 0x20u, /**< Uma medição, 1,0 lux              */
    BH1750_MODE_ONCE_H_RES2   = 0x21u, /**< Uma medição, 0,5 lux              */
    BH1750_MODE_ONCE_L_RES    = 0x23u, /**< Uma medição, 4,0 lux              */
} BH1750_Mode_t;

/* ─── Status de retorno ─────────────────────────────────────────────────── */
typedef enum {
    BH1750_OK           = 0x00, /**< Operação bem-sucedida                   */
    BH1750_ERR_I2C      = 0x01, /**< Falha na comunicação I²C                */
    BH1750_ERR_NOT_RDY  = 0x02, /**< Dado ainda não disponível (One Time)    */
    BH1750_ERR_SATURATED = 3,
    BH1750_ERR_CONFIG = 4,
    BH1750_ERR_RANGE = 5,
} BH1750_Status_t;

/* ─── Handle do sensor ───────────────────────────────────────────────────── */
typedef struct {

    /* ── Recursos de hardware ── */
    I2C_HandleTypeDef *hi2c;      /**< Handle do barramento I²C              */
    uint16_t           dev_addr;  /**< Endereço I²C deslocado (<<1) para HAL */
    BH1750_Mode_t      mode;      /**< Modo de operação ativo                */

    /* ── Fator de calibração experimental (lux → W/m²) ── */
    float k_factor;               /**< Padrão: BH1750_DEFAULT_K_FACTOR       */

    /* ── Dados lidos (válidos após BH1750_Read retornar BH1750_OK) ── */
    float illuminance;  /**< Iluminância em lux                              */
    float irradiance;   /**< Irradiância estimada em W/m² (lux * k_factor)  */

    uint8_t initialized;
    uint32_t measurement_start_ms;
} BH1750_Handle_t;

/* ─── Protótipos ─────────────────────────────────────────────────────────── */

/**
 * @brief  Inicializa o sensor: power-on, reset e inicia o modo de medição.
 *
 * @note   Após Init, aguarde ao menos 180 ms antes da primeira leitura
 *         (tempo de medição em H-Resolution). Em modo Continuous isso é
 *         necessário apenas uma vez; em One Time, a cada ciclo.
 *
 * @param  hdev      Ponteiro para o handle.
 * @param  hi2c      Handle do barramento I²C já inicializado.
 * @param  addr_7bit Endereço I²C de 7 bits (BH1750_ADDR_LOW ou _HIGH).
 * @param  mode      Modo de medição desejado.
 * @retval BH1750_OK | BH1750_ERR_I2C
 */
BH1750_Status_t BH1750_Init(BH1750_Handle_t   *hdev,
                             I2C_HandleTypeDef *hi2c,
                             uint8_t            addr_7bit,
                             BH1750_Mode_t      mode);

/**
 * @brief  Lê o valor de iluminância do sensor e calcula a irradiância.
 *
 * @note   Em modo Continuous: lê o resultado do último ciclo concluído.
 *         Em modo One Time: dispara nova medição, aguarda o tempo necessário
 *         e só então lê o resultado (bloqueante por até 180 ms).
 *
 * @param  hdev  Ponteiro para o handle inicializado.
 * @retval BH1750_OK | BH1750_ERR_I2C
 */
BH1750_Status_t BH1750_Read(BH1750_Handle_t *hdev);

/**
 * @brief  Ajusta o fator de calibração da conversão lux → W/m².
 *
 * @details Após campanha de comparação com a estação de referência da UFTM,
 *          calcule o fator empírico:
 *            k = irradiância_referência_W_m2 / leitura_lux_BH1750
 *          e passe-o aqui para corrigir todas as leituras subsequentes.
 *
 * @param  hdev     Ponteiro para o handle.
 * @param  k_factor Novo fator em W·m⁻²·lux⁻¹.
 *                  Passe BH1750_DEFAULT_K_FACTOR para restaurar o padrão.
 */
void BH1750_SetCalibration(BH1750_Handle_t *hdev, float k_factor);

/**
 * @brief  Coloca o sensor em Power Down (consumo < 1 µA).
 *         Chame BH1750_Init para reativar.
 *
 * @param  hdev  Ponteiro para o handle.
 * @retval BH1750_OK | BH1750_ERR_I2C
 */
BH1750_Status_t BH1750_PowerDown(BH1750_Handle_t *hdev);

/**
 * @brief  Retorna string descritiva do código de status.
 * @param  status  Código retornado pelas funções da lib.
 * @retval Ponteiro para string estática (não modificar).
 */
const char *BH1750_StatusStr(BH1750_Status_t status);


#ifdef __cplusplus
}
#endif

#endif /* BH1750_H */
