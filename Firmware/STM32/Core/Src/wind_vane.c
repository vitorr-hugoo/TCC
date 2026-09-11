/**
 * @file    wind_vane.c
 * @brief   Implementação do driver de biruta anemométrica para STM32 HAL.
 *
 * @details Consulte wind_vane.h para documentação completa da API pública,
 *          exemplo de uso e instruções de calibração.
 *
 * @version 1.0.0
 * @date    2025
 */

#include "wind_vane.h"

#include <string.h>   /* strncpy, memset */
#include <stddef.h>   /* NULL            */

/* =========================================================================
 * LUT padrão — 16 posições, 22,5° por passo
 *
 * ATENÇÃO: Os valores de adc_min / adc_max abaixo são ESTIMATIVAS para um
 * divisor resistivo típico com VDDA = 3,3 V e ADC de 12 bits (0–4095).
 * A tensão central de cada posição foi linearmente interpolada no intervalo.
 * As janelas têm ~128 contagens de largura; ajuste conforme a biruta real.
 *
 * Posição | Direção | Tensão típica (V) | ADC central (~) | Janela usada
 * --------|---------|-------------------|-----------------|-------------------
 *   0     |   N     |      0,00         |      0          |    0 –   63
 *   1     |  NNE    |      0,21         |    261          |  200 –  325
 *   2     |   NE    |      0,41         |    510          |  450 –  570
 *   3     |  ENE    |      0,62         |    771          |  710 –  835
 *   4     |   E     |      0,82         |   1020          |  960 – 1080
 *   5     |  ESE    |      1,03         |   1281          | 1220 – 1345
 *   6     |   SE    |      1,24         |   1543          | 1480 – 1607
 *   7     |  SSE    |      1,44         |   1792          | 1730 – 1855
 *   8     |   S     |      1,65         |   2048          | 1990 – 2110
 *   9     |  SSW    |      1,86         |   2315          | 2255 – 2375
 *  10     |   SW    |      2,06         |   2559          | 2500 – 2620
 *  11     |  WSW    |      2,27         |   2821          | 2760 – 2882
 *  12     |   W     |      2,48         |   3082          | 3020 – 3145
 *  13     |  WNW    |      2,68         |   3330          | 3270 – 3393
 *  14     |   NW    |      2,89         |   3591          | 3530 – 3655
 *  15     |  NNW    |      3,09         |   3840          | 3780 – 3900
 *
 * NOTA: A posição 0 (Norte) tem janela estreita próxima de 0 porque a tensão
 * de saída para 0° é tipicamente a mínima do divisor. Ajuste conforme medição.
 * ========================================================================= */
const WindVaneLUT_t WIND_VANE_DEFAULT_LUT[WIND_VANE_DEFAULT_LUT_SIZE] = {
    /*  adc_min, adc_max, degrees, label  */
    {      0u,     63u,     0u, "N"   },  /* Norte         */
    {    200u,    325u,    23u, "NNE" },  /* Norte-Nordeste */
    {    450u,    570u,    45u, "NE"  },  /* Nordeste       */
    {    710u,    835u,    68u, "ENE" },  /* Este-Nordeste  */
    {    960u,   1080u,    90u, "E"   },  /* Este           */
    {   1220u,   1345u,   113u, "ESE" },  /* Este-Sudeste   */
    {   1480u,   1607u,   135u, "SE"  },  /* Sudeste        */
    {   1730u,   1855u,   158u, "SSE" },  /* Sul-Sudeste    */
    {   1990u,   2110u,   180u, "S"   },  /* Sul            */
    {   2255u,   2375u,   203u, "SSW" },  /* Sul-Sudoeste   */
    {   2500u,   2620u,   225u, "SW"  },  /* Sudoeste       */
    {   2760u,   2882u,   248u, "WSW" },  /* Oeste-Sudoeste */
    {   3020u,   3145u,   270u, "W"   },  /* Oeste          */
    {   3270u,   3393u,   293u, "WNW" },  /* Oeste-Noroeste */
    {   3530u,   3655u,   315u, "NW"  },  /* Noroeste       */ //não encontra
    {   3780u,   3900u,   338u, "NNW" },  /* Norte-Noroeste */
};

/* =========================================================================
 * Estado interno (sem variáveis globais expostas)
 * ========================================================================= */

/** @brief Sinalizador: driver inicializado com sucesso. */
static uint8_t s_initialized = 0u;

/** @brief Cópia da configuração fornecida em Wind_Vane_Init(). */
static wind_vane_config_t s_cfg;

/** @brief Contador de outliers (amostras sem correspondência na LUT). */
static uint32_t s_error_count = 0u;

/* =========================================================================
 * Funções utilitárias internas (static)
 * ========================================================================= */

/**
 * @brief  Normaliza um ângulo arbitrário para o intervalo [0, 359].
 *
 * @details Aplica wrap-around correto mesmo para valores negativos, usando
 *          aritmética de inteiros de 32 bits para evitar overflow.
 *
 * @param[in] deg  Ângulo em graus (pode ser negativo ou > 359).
 * @retval         Ângulo normalizado em graus, no intervalo [0, 359].
 */
static uint16_t normalize_angle(int32_t deg)
{
    /* Garante resultado positivo antes do módulo */
    int32_t normalized = deg % 360;
    if (normalized < 0) {
        normalized += 360;
    }
    return (uint16_t)normalized;
}

/**
 * @brief  Mapeia um valor bruto do ADC para graus via LUT.
 *
 * @details Percorre todas as entradas da LUT procurando a janela
 *          [adc_min, adc_max] que contenha o valor `adc_raw`.
 *          Se nenhuma janela corresponder, incrementa o contador de erros
 *          interno e retorna HAL_ERROR.
 *
 * @param[in]  adc_raw     Valor bruto lido do ADC (0–4095 para 12 bits).
 * @param[out] degrees_out Ponteiro para receber os graus correspondentes.
 *
 * @retval HAL_OK    Mapeamento realizado com sucesso.
 * @retval HAL_ERROR Valor fora de todas as janelas da LUT (outlier).
 */
static HAL_StatusTypeDef Wind_Vane_MapAdcToDegrees(uint16_t  adc_raw,
                                                    uint16_t *degrees_out)
{
    uint8_t i;

    for (i = 0u; i < s_cfg.lut_size; i++) {
        if ((adc_raw >= s_cfg.lut[i].adc_min) &&
            (adc_raw <= s_cfg.lut[i].adc_max)) {
            *degrees_out = s_cfg.lut[i].degrees;
            return HAL_OK;
        }
    }

    /* Nenhuma janela encontrada: outlier ou sensor falho */
    s_error_count++;
    *degrees_out = WIND_VANE_INVALID_DEGREES;
    return HAL_ERROR;
}

/**
 * @brief  Calcula a moda de um array de ângulos em graus.
 *
 * @details Retorna o valor que aparece com maior frequência. Em caso de
 *          empate, retorna o primeiro valor de maior frequência (em ordem
 *          de aparição). Complexidade: O(N²), aceitável para N ≤ 16.
 *          Ignora entradas com valor WIND_VANE_INVALID_DEGREES.
 *
 * @param[in] samples     Array de ângulos em graus (saída da LUT).
 * @param[in] num_samples Número de elementos válidos no array.
 *
 * @retval Ângulo modal em graus, ou WIND_VANE_INVALID_DEGREES se não houver
 *         nenhuma amostra válida.
 */
static uint16_t Wind_Vane_Mode(const uint16_t *samples, uint8_t num_samples)
{
    uint8_t  i;
    uint8_t  j;
    uint8_t  count;
    uint8_t  best_count = 0u;
    uint16_t best_value = WIND_VANE_INVALID_DEGREES;

    for (i = 0u; i < num_samples; i++) {
        if (samples[i] == WIND_VANE_INVALID_DEGREES) {
            continue; /* ignora amostras inválidas */
        }

        count = 0u;
        for (j = 0u; j < num_samples; j++) {
            if (samples[j] == samples[i]) {
                count++;
            }
        }

        if (count > best_count) {
            best_count = count;
            best_value = samples[i];
        }
    }

    return best_value;
}

static void Wind_Vane_EnableAdcClock(ADC_HandleTypeDef *hadc)
{
    if (hadc == NULL) {
        return;
    }

    if (hadc->Instance == ADC1) {
        __HAL_RCC_ADC1_CLK_ENABLE();
    }
#if defined(ADC2)
    else if (hadc->Instance == ADC2) {
        __HAL_RCC_ADC2_CLK_ENABLE();
    }
#endif
#if defined(ADC3)
    else if (hadc->Instance == ADC3) {
        __HAL_RCC_ADC3_CLK_ENABLE();
    }
#endif
}

static void Wind_Vane_DisableAdcClock(ADC_HandleTypeDef *hadc)
{
    if (hadc == NULL) {
        return;
    }

    if (hadc->Instance == ADC1) {
        __HAL_RCC_ADC1_CLK_DISABLE();
    }
#if defined(ADC2)
    else if (hadc->Instance == ADC2) {
        __HAL_RCC_ADC2_CLK_DISABLE();
    }
#endif
#if defined(ADC3)
    else if (hadc->Instance == ADC3) {
        __HAL_RCC_ADC3_CLK_DISABLE();
    }
#endif
}

/* =========================================================================
 * Implementação da API pública
 * ========================================================================= */

/**
 * @brief  Inicializa o driver da biruta.
 */
HAL_StatusTypeDef Wind_Vane_Init(const wind_vane_config_t *cfg)
{
    ADC_ChannelConfTypeDef sConfig;
    HAL_StatusTypeDef      status;

    /* --- Validação de parâmetros --- */
    if (cfg == NULL) {
        return HAL_ERROR;
    }
    if (cfg->hadc == NULL) {
        return HAL_ERROR;
    }
    if (cfg->lut == NULL) {
        return HAL_ERROR;
    }
    if ((cfg->lut_size == 0u) || (cfg->lut_size > WIND_VANE_NUM_POSITIONS)) {
        return HAL_ERROR;
    }
    if ((cfg->num_samples == 0u) || (cfg->num_samples > WIND_VANE_MAX_SAMPLES)) {
        return HAL_ERROR;
    }

    /* --- Armazena configuração interna --- */
    s_cfg         = *cfg;
    s_error_count = 0u;
    s_initialized = 0u;

    /* --- Configura canal ADC --- */
    memset(&sConfig, 0, sizeof(sConfig));
    sConfig.Channel      = cfg->channel;
    sConfig.Rank         = 1;
    sConfig.SamplingTime = cfg->sampling_time;

    status = HAL_ADC_ConfigChannel(cfg->hadc, &sConfig);
    if (status != HAL_OK) {
        return HAL_ERROR;
    }

    s_initialized = 1u;
    return HAL_OK;
}

/**
 * @brief  Realiza uma leitura completa da direção do vento.
 */
HAL_StatusTypeDef Wind_Vane_Read(uint16_t *degrees_out)
{
    uint8_t           i;
    uint8_t           valid_samples = 0u;
    uint16_t          adc_raw       = 0u;
    uint16_t          deg_sample    = 0u;
    uint16_t          samples[WIND_VANE_MAX_SAMPLES];
    uint16_t          modal_deg;
    HAL_StatusTypeDef status;

    if ((s_initialized == 0u) || (degrees_out == NULL)) {
        return HAL_ERROR;
    }

    /* Inicializa array de amostras com valor sentinela */
    for (i = 0u; i < WIND_VANE_MAX_SAMPLES; i++) {
        samples[i] = WIND_VANE_INVALID_DEGREES;
    }

    /* --- 1. Liga o clock do ADC --- */
    //__HAL_RCC_ADC1_CLK_ENABLE();
    Wind_Vane_EnableAdcClock(s_cfg.hadc);
    /* --- 2. Burst de aquisição --- */
    for (i = 0u; i < s_cfg.num_samples; i++) {

        /* Inicia conversão */
        status = HAL_ADC_Start(s_cfg.hadc);
        if (status != HAL_OK) {
            //__HAL_RCC_ADC1_CLK_DISABLE();
        	Wind_Vane_DisableAdcClock(s_cfg.hadc);
            return HAL_ERROR;
        }

        /* Aguarda conclusão da conversão */
        status = HAL_ADC_PollForConversion(s_cfg.hadc, WIND_VANE_ADC_TIMEOUT_MS);
        if (status == HAL_TIMEOUT) {
            /* Interrompe burst, desliga ADC e propaga timeout */
            (void)HAL_ADC_Stop(s_cfg.hadc);
            //__HAL_RCC_ADC1_CLK_DISABLE();
            Wind_Vane_DisableAdcClock(s_cfg.hadc);
            return HAL_TIMEOUT;
        }
        if (status != HAL_OK) {
            (void)HAL_ADC_Stop(s_cfg.hadc);
            //__HAL_RCC_ADC1_CLK_DISABLE();
            Wind_Vane_DisableAdcClock(s_cfg.hadc);
            return HAL_ERROR;
        }

        /* Lê valor bruto */
        adc_raw = (uint16_t)HAL_ADC_GetValue(s_cfg.hadc);

        /* Para o ADC antes do próximo ciclo */
        status = HAL_ADC_Stop(s_cfg.hadc);
        if (status != HAL_OK) {
            //__HAL_RCC_ADC1_CLK_DISABLE();
        	Wind_Vane_DisableAdcClock(s_cfg.hadc);
            return HAL_ERROR;
        }

        /* Mapeia amostra para graus via LUT */
        if (Wind_Vane_MapAdcToDegrees(adc_raw, &deg_sample) == HAL_OK) {
            samples[valid_samples] = deg_sample;
            valid_samples++;
        }
        /* outliers já foram contabilizados internamente */

        /* Intervalo entre amostras (não aplicar após a última) */
        if (i < (s_cfg.num_samples - 1u)) {
            HAL_Delay(s_cfg.sample_interval_ms);
        }
    }

    /* --- 3. Desliga o clock do ADC --- */
    //__HAL_RCC_ADC1_CLK_DISABLE();
    Wind_Vane_DisableAdcClock(s_cfg.hadc);

    /* --- 4. Verifica se há amostras válidas --- */
    if (valid_samples == 0u) {
        return HAL_ERROR;
    }

    /* --- 5. Filtro de moda --- */
    modal_deg = Wind_Vane_Mode(samples, valid_samples);
    if (modal_deg == WIND_VANE_INVALID_DEGREES) {
        return HAL_ERROR;
    }

    /* --- 6. Aplica offset de Norte com wrap-around --- */
    *degrees_out = normalize_angle((int32_t)modal_deg +
                                   (int32_t)s_cfg.north_offset_deg);

    return HAL_OK;
}

/**
 * @brief  Retorna o rótulo textual da direção mais próxima na LUT.
 */

HAL_StatusTypeDef Wind_Vane_ReadRaw(uint16_t *adc_raw_out)
{
    HAL_StatusTypeDef status;

    if ((s_initialized == 0u) || (adc_raw_out == NULL)) {
        return HAL_ERROR;
    }

    Wind_Vane_EnableAdcClock(s_cfg.hadc);

    status = HAL_ADC_Start(s_cfg.hadc);
    if (status != HAL_OK) {
        Wind_Vane_DisableAdcClock(s_cfg.hadc);
        return HAL_ERROR;
    }

    status = HAL_ADC_PollForConversion(s_cfg.hadc, WIND_VANE_ADC_TIMEOUT_MS);
    if (status == HAL_TIMEOUT) {
        (void)HAL_ADC_Stop(s_cfg.hadc);
        Wind_Vane_DisableAdcClock(s_cfg.hadc);
        return HAL_TIMEOUT;
    }
    if (status != HAL_OK) {
        (void)HAL_ADC_Stop(s_cfg.hadc);
        Wind_Vane_DisableAdcClock(s_cfg.hadc);
        return HAL_ERROR;
    }

    *adc_raw_out = (uint16_t)HAL_ADC_GetValue(s_cfg.hadc);

    status = HAL_ADC_Stop(s_cfg.hadc);
    Wind_Vane_DisableAdcClock(s_cfg.hadc);

    return status;
}
void Wind_Vane_GetLabel(uint16_t degrees, char *buf, uint8_t buf_size)
{
    uint8_t  i;
    uint8_t  best_idx   = 0u;
    uint32_t best_dist  = 0xFFFFFFFFu;
    uint32_t dist;
    int32_t  diff;

    if ((buf == NULL) || (buf_size == 0u) || (s_initialized == 0u)) {
        return;
    }

    for (i = 0u; i < s_cfg.lut_size; i++) {
        /* Distância circular entre `degrees` e o centro da posição LUT */
        diff = (int32_t)degrees - (int32_t)s_cfg.lut[i].degrees;

        /* Normaliza a diferença para [-180, +180] */
        if (diff > 180) {
            diff -= 360;
        } else if (diff < -180) {
            diff += 360;
        }

        /* Valor absoluto sem float */
        dist = (diff >= 0) ? (uint32_t)diff : (uint32_t)(-diff);

        if (dist < best_dist) {
            best_dist = dist;
            best_idx  = i;
        }
    }

    if (s_cfg.lut[best_idx].label != NULL) {
        strncpy(buf, s_cfg.lut[best_idx].label, (size_t)(buf_size - 1u));
    } else {
        strncpy(buf, "???", (size_t)(buf_size - 1u));
    }

    /* Garante terminação nula */
    buf[buf_size - 1u] = '\0';
}

/**
 * @brief  Atualiza o offset de calibração de Norte em tempo de execução.
 */
void Wind_Vane_SetNorthOffset(int16_t offset_degrees)
{
    s_cfg.north_offset_deg = offset_degrees;
}

/**
 * @brief  Retorna o contador de erros de mapeamento LUT acumulados.
 */
uint32_t Wind_Vane_GetErrorCount(void)
{
    return s_error_count;
}
