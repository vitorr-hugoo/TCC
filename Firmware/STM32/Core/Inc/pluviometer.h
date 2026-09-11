/**
 * @file    pluviometer.h
 * @brief   Interface pública do módulo de pluviômetro de báscula (WH-SP-RG ou compatível).
 *
 * @details Módulo responsável por medir a precipitação pluviométrica acumulada
 *          via contagem de basculadas (pulsos EXTI em PA1). Cada fechamento do
 *          reed switch corresponde a um volume fixo de água coletada pelo funil
 *          (fator de conversão: PLUV_MM_PER_TIP).
 *
 *          Diferença fundamental em relação ao anemômetro:
 *          O pluviômetro é um CONTADOR ACUMULATIVO — não usa janela de integração
 *          com timer. Cada basculada incrementa um totalizador persistente que só
 *          é zerado por chamada explícita a pluviometer_reset(). O loop principal
 *          lê a chuva acumulada no intervalo entre dois ciclos de medição calculando
 *          a diferença de contadores (delta).
 *
 * @author  TCC — Estação Meteorológica Automática IoT
 * @version 1.0.0
 *
 * Fluxo de uso:
 *  1. pluviometer_init()                         — uma vez, após MX_GPIO_Init()
 *  2. (ISR EXTI) pluviometer_tip_callback()      — conta basculadas continuamente
 *  3. pluviometer_read(&result)                  — lê acumulado e calcula mm
 *  4. pluviometer_reset()                        — zera o totalizador (opcional,
 *                                                   ex: a cada 24 h ou por ciclo)
 *
 * Estratégia de delta por ciclo:
 *  A cada ciclo de medição, use pluviometer_get_delta() para obter apenas a
 *  chuva do intervalo atual, sem perder o acumulado total.
 */

#ifndef PLUVIOMETER_H
#define PLUVIOMETER_H

#ifdef __cplusplus
extern "C" {
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
 * @brief Janela mínima entre duas basculadas válidas [ms].
 *        Uma báscula física leva no mínimo ~150–200 ms para completar o
 *        movimento mecânico. 200 ms é margem segura que rejeita bounces
 *        sem risco de perder eventos reais mesmo em chuva forte.
 *        Chuva máxima típica (~300 mm/h) → ~1 basculada/s (período = 1000 ms)
 *        para funis padrão de 0,2794 mm/tip.
 */
#define PLUV_DEBOUNCE_MS        200u

/**
 * @brief Volume de precipitação correspondente a uma basculada [mm].
 *        Valor padrão do WH-SP-RG / Sparkfun DS-15901: 0,2794 mm por tip.
 *        Ajustar conforme calibração do sensor utilizado.
 *
 * @note  O valor 0.2794f é equivalente a 1 polegada / 91 tips (especificação
 *        do fabricante). Multiplica-se pela área do funil internamente ao fator.
 */
#define PLUV_MM_PER_TIP         0.2794f

/**
 * @brief Valor máximo do totalizador de basculadas antes de overflow.
 *        Com uint32_t e 0,2794 mm/tip: 4.294.967.295 × 0,2794 ≈ 1.200.000 mm
 *        de chuva acumulada antes de transbordar — na prática, o reset periódico
 *        (diário) ocorrerá muito antes.
 *        Definido como UINT32_MAX para uso em verificação defensiva futura.
 */
#define PLUV_TIP_COUNT_MAX      (0xFFFFFFFFu)

/* -------------------------------------------------------------------------- */
/* Tipos públicos                                                              */
/* -------------------------------------------------------------------------- */

/**
 * @brief Estrutura com o resultado de uma leitura do pluviômetro.
 */
typedef struct {
    float    rain_mm;         /**< Precipitação acumulada desde o último reset [mm]       */
    float    delta_mm;        /**< Precipitação do intervalo atual (desde última leitura) [mm] */
    uint32_t tip_count;       /**< Número bruto de basculadas desde o último reset        */
    uint32_t delta_tips;      /**< Basculadas do intervalo atual (desde última leitura)   */
    uint8_t  valid;           /**< 1 = leitura calculada com sucesso; 0 = pendente/vazia  */
} Pluviometer_Data_t;

/* -------------------------------------------------------------------------- */
/* Protótipos de funções públicas                                              */
/* -------------------------------------------------------------------------- */

/**
 * @brief  Inicializa as variáveis internas do módulo pluviômetro.
 *
 * @details Deve ser chamada uma única vez, após MX_GPIO_Init(), antes do
 *          loop principal. Zera o totalizador, o snapshot de delta e o cache
 *          de último resultado. NÃO habilita interrupção — a EXTI é habilitada
 *          automaticamente pelo CubeMX via MX_GPIO_Init().
 */
void pluviometer_init(void);

/**
 * @brief  Lê o totalizador de basculadas e calcula a precipitação acumulada.
 *
 * @details Captura atomicamente o valor atual de tip_count (seção crítica),
 *          calcula rain_mm = tip_count × PLUV_MM_PER_TIP e calcula o delta
 *          em relação à leitura anterior (delta_mm, delta_tips). Atualiza o
 *          snapshot interno para o próximo cálculo de delta.
 *          Seta result->valid = 1 sempre que executada corretamente.
 *          Zero basculadas (chuva = 0,0 mm) é resultado VÁLIDO — não é erro.
 *
 * @param  result  Ponteiro para a struct de saída; não pode ser NULL.
 */
void pluviometer_read(Pluviometer_Data_t *result);

/**
 * @brief  Callback de contagem de basculadas — chamado pela ISR do EXTI.
 *
 * @details Aplica debounce por software (PLUV_DEBOUNCE_MS) e, se o intervalo
 *          desde a última basculada válida for suficiente, incrementa o
 *          totalizador tip_count. NÃO realiza nenhum cálculo de ponto flutuante.
 *          DEVE ser chamada dentro de HAL_GPIO_EXTI_Callback,
 *          na guarda if (GPIO_Pin == PLUV_RAIN_Pin).
 */
void pluviometer_tip_callback(void);

/**
 * @brief  Zera o totalizador de basculadas e o snapshot de delta.
 *
 * @details Usa seção crítica para garantir atomicidade. Deve ser chamada
 *          periodicamente (ex: a cada 24 h pelo RTC, ou no início de cada
 *          período de monitoramento). Não afeta o cache de last_result.
 */
void pluviometer_reset(void);

/**
 * @brief  Retorna o último resultado calculado por pluviometer_read().
 *
 * @details Retorna cópia por valor da struct interna. Se pluviometer_read()
 *          ainda não foi chamada, retorna struct com todos os campos zerados
 *          e valid = 0.
 *
 * @return  Última Pluviometer_Data_t calculada, ou struct zerada (valid = 0).
 */
Pluviometer_Data_t pluviometer_get_last(void);

/**
 * @brief  Retorna o número bruto de basculadas acumuladas desde o último reset.
 *
 * @details Leitura atômica do totalizador. Útil para inspeção rápida sem
 *          precisar preencher a struct completa. NÃO consome nem modifica
 *          nenhum estado interno.
 *
 * @return  Valor atual do totalizador tip_count.
 */
uint32_t pluviometer_get_tip_count(void);
void pluviometer_get_diagnostics(uint32_t *rejected, uint32_t *last_tip_ms);

#ifdef __cplusplus
}
#endif

#endif /* PLUVIOMETER_H */
