/**
 * @file    anemometer.h
 * @brief   Interface pública do módulo de anemômetro de conchas (WH-SP-WS01).
 *
 * @details Módulo responsável por medir a velocidade do vento via contagem de
 *          pulsos em janela de integração de 1 segundo, usando EXTI (PC7) e
 *          TIM6 como base de tempo. Compatível com STM32F446RET6 + HAL.
 *
 * @author  TCC — Estação Meteorológica Automática IoT
 * @version 1.0.0
 *
 * Fluxo de uso:
 *  1. anemometer_init()                        — uma vez, após MX_GPIO_Init() e MX_TIM6_Init()
 *  2. anemometer_start_sampling(&htim6)         — inicia janela de 1 s
 *  3. (ISR EXTI) anemometer_pulse_callback()   — conta pulsos
 *  4. (ISR TIM6) anemometer_stop_sampling()    — congela contagem
 *  5. anemometer_is_ready()                    — verifica se janela encerrou
 *  6. anemometer_calculate(ANEM_SAMPLE_WINDOW_MS, &result) — calcula velocidade
 */

#ifndef ANEMOMETER_H
#define ANEMOMETER_H

#ifdef __cplusplus
extern "C"
{
#endif

/* -------------------------------------------------------------------------- */
/* Dependências                                                                */
/* -------------------------------------------------------------------------- */
#include "stm32f4xx_hal.h"
#include <stdint.h>

/* -------------------------------------------------------------------------- */
/* Macros de configuração                                                      */
/* -------------------------------------------------------------------------- */

/**
 * @brief Janela mínima entre dois pulsos válidos [ms].
 *        Velocidade máxima física (~50 m/s) gera ~25 Hz (período ≈ 40 ms).
 *        20 ms é margem conservadora; filtra bounces residuais do reed switch.
 */
#define ANEM_DEBOUNCE_MS 20u

/**
 * @brief Duração da janela de integração de pulsos [ms].
 *        Deve coincidir com a configuração do ARR/PSC do TIM6.
 */
#define ANEM_SAMPLE_WINDOW_MS  1000u
#define ANEM_TIMEOUT_MARGIN_MS 1000u

/**
 * @brief Número de pulsos gerados por rotação completa do eixo (WH-SP-WS01).
 *        O imã fecha o reed switch duas vezes por volta.
 */
#define ANEM_PULSES_PER_REV 2u

/**
 * @brief Fator de conversão do fabricante: 1 Hz de pulsos = 2,4 km/h.
 *        Equivale a: 1 fechamento/s = 1,2 km/h; como há 2 pulsos/rev → 2,4 km/h/Hz.
 */
#define ANEM_KMH_PER_HZ 2.4f

    /* -------------------------------------------------------------------------- */
    /* Tipos públicos                                                              */
    /* -------------------------------------------------------------------------- */

    /**
 * @brief Estrutura com o resultado de uma janela de amostragem.
 */
    typedef struct
    {
        float speed_ms;       /**< Velocidade calculada em metros por segundo [m/s]        */
        float speed_kmh;      /**< Velocidade calculada em quilômetros por hora [km/h]     */
        uint32_t pulse_count; /**< Quantidade bruta de pulsos capturados na janela         */
        uint8_t valid; /**< 1 = leitura disponível e calculada; 0 = leitura invalida/pendente  */
    } Anemometer_Data_t;

    /* -------------------------------------------------------------------------- */
    /* Protótipos de funções públicas                                              */
    /* -------------------------------------------------------------------------- */

    /**
 * @brief  Inicializa as variáveis internas do módulo.
 *
 * @details Deve ser chamada uma única vez, após MX_GPIO_Init() e MX_TIM6_Init(),
 *          antes do início do loop principal. NÃO inicia o timer nem habilita
 *          a interrupção EXTI — apenas garante estado interno limpo e definido.
 */
    void anemometer_init(void);

    /**
 * @brief  Inicia uma nova janela de amostragem de 1 segundo.
 *
 * @details - Zera o contador de pulsos e o timestamp do último pulso válido.
 *          - Limpa o flag de resultado pronto.
 *          - Inicia o TIM6 em modo interrupção (HAL_TIM_Base_Start_IT).
 *          Chame uma vez por ciclo de medição, no início do burst.
 *
 * @param  htim  Ponteiro para o handle do TIM6 (htim6, declarado em main.c).
 */
    HAL_StatusTypeDef anemometer_start_sampling(TIM_HandleTypeDef *htim);

    /**
 * @brief  Encerra a janela de amostragem e congela o contador de pulsos.
 *
 * @details - Para o TIM6 (HAL_TIM_Base_Stop_IT).
 *          - Copia pulse_count para frozen_count em seção crítica (__disable_irq).
 *          - Sinaliza que o resultado está pronto para cálculo.
 *          DEVE ser chamada exclusivamente dentro de HAL_TIM_PeriodElapsedCallback,
 *          na guarda if (htim->Instance == TIM6).
 *
 * @param  htim  Ponteiro para o handle do TIM6.
 */
    void anemometer_stop_sampling(TIM_HandleTypeDef *htim);
    /* Service in main after other reads; HAL_BUSY means window still pending. */
    HAL_StatusTypeDef anemometer_service(TIM_HandleTypeDef *htim, uint32_t now);
    void anemometer_get_diagnostics(uint32_t *accepted, uint32_t *rejected,
                                    uint32_t *last_pulse_ms);

    /**
 * @brief  Calcula a velocidade do vento a partir dos pulsos congelados.
 *
 * @details Converte o valor de frozen_count em frequência (Hz) e aplica o
 *          fator de conversão ANEM_KMH_PER_HZ. Preenche todos os campos da
 *          struct de saída, incluindo valid = 1. Calmaria (pulse_count == 0)
 *          é um resultado válido: speed_ms = speed_kmh = 0.0f, valid = 1.
 *          Se a janela nao concluiu, devolve valid = 0 sem reutilizar cache.
 *
 * @param  window_ms  Duração real da janela de integração [ms] (use ANEM_SAMPLE_WINDOW_MS).
 * @param  result     Ponteiro para a struct de saída; não pode ser NULL.
 */
    void anemometer_calculate(uint32_t window_ms, Anemometer_Data_t *result);

    /**
 * @brief  Callback de contagem de pulsos — chamado pela ISR de EXTI.
 *
 * @details Aplica debounce por software (ANEM_DEBOUNCE_MS) comparando
 *          HAL_GetTick() com o timestamp do último pulso válido. Se o intervalo
 *          for suficiente, incrementa o contador volátil interno.
 *          NÃO realiza nenhum cálculo de ponto flutuante.
 *          DEVE ser chamada dentro de HAL_GPIO_EXTI_Callback,
 *          na guarda if (GPIO_Pin == ANEM_SPEED_Pin).
 */
    void anemometer_pulse_callback(void);

    /**
 * @brief  Verifica se uma janela de amostragem foi concluída.
 *
 * @details NÃO consome o flag interno — apenas o lê. Use anemometer_calculate()
 *          para processar e consumir o resultado.
 *
 * @return  1  se o resultado estiver disponível para leitura.
 * @return  0  caso contrário (janela em andamento ou módulo não iniciado).
 */
    uint8_t anemometer_is_ready(void);

    /**
 * @brief  Retorna o último resultado calculado (cópia da struct interna).
 *
 * @details Se nenhuma janela foi concluída e calculada ainda, retorna uma
 *          struct com todos os campos zerados e valid = 0.
 *
 * @return  Cópia por valor da última Anemometer_Data_t calculada.
 */
    Anemometer_Data_t anemometer_get_last(void);

#ifdef __cplusplus
}
#endif

#endif /* ANEMOMETER_H */
