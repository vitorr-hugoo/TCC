/**
 * @file    wind_vane.h
 * @brief   Driver de biruta anemométrica (divisor resistivo, 16 posições) para
 *          STM32 HAL — projeto de estação meteorológica IoT.
 *
 * @details
 * Estratégia de aquisição:
 *  - O ADC é energizado apenas durante o burst de leitura (Race-to-Sleep).
 *  - São coletadas `num_samples` amostras brutas; cada amostra é mapeada para
 *    graus via LUT de janelas [adc_min, adc_max]; o resultado final é a **moda**
 *    das amostras mapeadas (filtragem por votação).
 *  - O offset de Norte é aplicado com wrap-around garantido em 0–359°.
 *
 * @par Exemplo de uso
 * @code
 * #include "wind_vane.h"
 *
 * // --- Inicialização (chamar uma vez no setup) ---
 * wind_vane_config_t vane_cfg = {
 *     .hadc             = &hadc1,
 *     .channel          = ADC_CHANNEL_0,
 *     .sampling_time    = ADC_SAMPLETIME_84CYCLES,
 *     .num_samples      = 8u,
 *     .sample_interval_ms = 5u,
 *     .north_offset_deg = 0,          // ajustar após calibração
 *     .lut              = WIND_VANE_DEFAULT_LUT,
 *     .lut_size         = WIND_VANE_DEFAULT_LUT_SIZE,
 * };
 *
 * if (Wind_Vane_Init(&vane_cfg) != HAL_OK) {
 *     // tratar erro de inicialização
 * }
 *
 * // --- Ciclo periódico (ex.: tick de 1 s disparado por RTC) ---
 * uint16_t direction_deg = 0u;
 * char     label[WIND_VANE_LABEL_BUF_SIZE];
 *
 * HAL_StatusTypeDef st = Wind_Vane_Read(&direction_deg);
 * if (st == HAL_OK) {
 *     Wind_Vane_GetLabel(direction_deg, label, sizeof(label));
 *
 *     // Integração com módulo de telemetria (implementado na camada de app):
 *     // Telemetry_SetWindDirection(direction_deg, label);
 * }
 * @endcode
 *
 * @note Compatível com C99 estrito. Compilável com -Wall -Wextra -Werror.
 *       Sem alocação dinâmica, sem FreeRTOS, sem float/double.
 *
 * @version 1.0.0
 * @date    2025
 */

#ifndef WIND_VANE_H
#define WIND_VANE_H

/* =========================================================================
 * Includes
 * ========================================================================= */
#include "stm32f4xx_hal.h" /* HAL_StatusTypeDef, ADC_HandleTypeDef, etc. */
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* =========================================================================
 * Constantes públicas
 * ========================================================================= */

/** @brief Número de posições da biruta (16 direções × 22,5°). */
#define WIND_VANE_NUM_POSITIONS (16u)

/** @brief Tamanho máximo recomendado para o buffer de label textual. */
#define WIND_VANE_LABEL_BUF_SIZE (8u)

/** @brief Número máximo de amostras por burst (limite interno do filtro de moda). */
#define WIND_VANE_MAX_SAMPLES (16u)

/** @brief Valor sentinela para ângulo inválido. */
#define WIND_VANE_INVALID_DEGREES (0xFFFFu)

/** @brief Timeout ADC por amostra em milissegundos. */
#define WIND_VANE_ADC_TIMEOUT_MS (10u)

    /* =========================================================================
 * Tipos públicos
 * ========================================================================= */

    /**
 * @brief Entrada da LUT: janela de decisão ADC → direção do vento.
 *
 * @note  Os valores de adc_min / adc_max são ILUSTRATIVOS na tabela padrão.
 *        Devem ser medidos na bancada com a biruta real (ver "Procedimento de
 *        Calibração" no final da documentação).
 */
    typedef struct
    {
        uint16_t adc_min;  /**< Limiar inferior da janela ADC (inclusive). */
        uint16_t adc_max;  /**< Limiar superior da janela ADC (inclusive). */
        uint16_t degrees;  /**< Direção correspondente em graus (0–359). */
        const char *label; /**< Rótulo textual, ex.: "N", "NNE", "NE", … */
    } WindVaneLUT_t;

    /**
 * @brief Estrutura de configuração da biruta.
 *
 * @note  Preencher antes de chamar Wind_Vane_Init().
 */
    typedef struct
    {
        ADC_HandleTypeDef *hadc;     /**< Handle do ADC (HAL). */
        uint32_t channel;            /**< Canal ADC (ex.: ADC_CHANNEL_0). */
        uint32_t sampling_time;      /**< Tempo de amostragem ADC (ex.: ADC_SAMPLETIME_84CYCLES). */
        uint8_t num_samples;         /**< Amostras por burst (1–WIND_VANE_MAX_SAMPLES). */
        uint16_t sample_interval_ms; /**< Intervalo entre amostras dentro do burst (ms). */
        int16_t
            north_offset_deg; /**< Offset de calibração de Norte em graus (pode ser negativo). */
        const WindVaneLUT_t *lut; /**< Ponteiro para a tabela LUT. */
        uint8_t lut_size;         /**< Número de entradas na LUT. */
    } wind_vane_config_t;

/* =========================================================================
 * LUT padrão (valores ILUSTRATIVOS — calibrar na bancada!)
 * ========================================================================= */

/**
 * @brief Número de entradas da LUT padrão.
 */
#define WIND_VANE_DEFAULT_LUT_SIZE (16u)

    /**
 * @brief LUT padrão com 16 posições.
 *
 * @warning Os valores de adc_min / adc_max abaixo são ESTIMATIVAS para um
 *          divisor resistivo típico com VDDA = 3,3 V e ADC de 12 bits (0–4095).
 *          Eles DEVEM ser substituídos pelos valores reais medidos na bancada.
 *          Consulte a seção "Procedimento de Calibração" na documentação.
 */
    extern const WindVaneLUT_t WIND_VANE_DEFAULT_LUT[WIND_VANE_DEFAULT_LUT_SIZE];

    /* =========================================================================
 * API pública
 * ========================================================================= */

    /**
 * @brief  Inicializa o driver da biruta.
 *
 * @details Valida os parâmetros de configuração, configura o canal ADC via
 *          HAL_ADC_ConfigChannel e armazena a configuração internamente.
 *          Deve ser chamada uma única vez, antes de qualquer leitura.
 *
 * @param[in] cfg  Ponteiro para a estrutura de configuração preenchida.
 *                 Não deve ser NULL.
 *
 * @retval HAL_OK     Inicialização bem-sucedida.
 * @retval HAL_ERROR  Parâmetros inválidos ou falha na configuração do ADC.
 */
    HAL_StatusTypeDef Wind_Vane_Init(const wind_vane_config_t *cfg);

    /**
 * @brief  Realiza uma leitura completa da direção do vento.
 *
 * @details
 *  1. Habilita o clock do ADC.
 *  2. Coleta `num_samples` amostras brutas com intervalo `sample_interval_ms`.
 *  3. Mapeia cada amostra para graus via LUT.
 *  4. Aplica filtro de moda sobre os graus mapeados.
 *  5. Aplica o offset de Norte com wrap-around.
 *  6. Desabilita o clock do ADC.
 *  7. Armazena o resultado em *degrees_out.
 *
 * @note  Usa HAL_Delay — não chamar de contexto de ISR.
 *
 * @param[out] degrees_out  Ponteiro para receber a direção em graus (0–359).
 *                          Não deve ser NULL.
 *
 * @retval HAL_OK      Leitura concluída com sucesso.
 * @retval HAL_ERROR   Falha de ADC ou nenhuma amostra mapeável pela LUT.
 * @retval HAL_TIMEOUT Timeout de conversão ADC; burst interrompido.
 */
    HAL_StatusTypeDef Wind_Vane_Read(uint16_t *degrees_out);

    /**
 * @brief  Retorna uma leitura bruta do ADC da biruta.
 *
 * @param[out] adc_raw_out Ponteiro para receber o valor bruto do ADC.
 * @retval HAL_OK      Leitura concluída com sucesso.
 * @retval HAL_ERROR   Erro de parâmetro, inicialização ou ADC.
 * @retval HAL_TIMEOUT Timeout na conversão ADC.
 */
    HAL_StatusTypeDef Wind_Vane_ReadRaw(uint16_t *adc_raw_out);

    /**
 * @brief  Retorna o rótulo textual da direção mais próxima na LUT.
 *
 * @details Percorre a LUT e seleciona a entrada cuja direção (em graus) é
 *          mais próxima do valor fornecido, considerando wrap-around circular.
 *          O resultado é copiado com segurança para `buf` (strncpy + terminador).
 *
 * @param[in]  degrees   Ângulo em graus (0–359).
 * @param[out] buf       Buffer de destino para o rótulo.
 * @param[in]  buf_size  Tamanho do buffer (mínimo: WIND_VANE_LABEL_BUF_SIZE).
 */
    void Wind_Vane_GetLabel(uint16_t degrees, char *buf, uint8_t buf_size);

    /**
 * @brief  Atualiza o offset de calibração de Norte em tempo de execução.
 *
 * @details Permite ajuste fino sem reinicialização do driver.
 *          O novo offset é aplicado na próxima chamada a Wind_Vane_Read().
 *
 * @param[in] offset_degrees  Offset em graus (pode ser negativo).
 */
    void Wind_Vane_SetNorthOffset(int16_t offset_degrees);

    /**
 * @brief  Retorna o contador de erros de mapeamento LUT acumulados.
 *
 * @details Incrementado a cada amostra que não encontra janela na LUT
 *          (outlier / sensor falho). Útil para diagnóstico em campo.
 *
 * @retval Número de outliers detectados desde o último Wind_Vane_Init().
 */
    uint32_t Wind_Vane_GetErrorCount(void);

#ifdef __cplusplus
}
#endif

#endif /* WIND_VANE_H */
