/**
 * @file    pluviometer.c
 * @brief   Implementação do módulo de pluviômetro de báscula (WH-SP-RG ou compatível).
 *
 * @details Contagem acumulativa de basculadas via EXTI (PA1 / borda de descida).
 *          NÃO usa timer de janela — o totalizador é persistente entre ciclos
 *          e só é zerado por chamada explícita a pluviometer_reset().
 *          O campo delta_mm/delta_tips informa apenas a precipitação do intervalo
 *          entre duas chamadas consecutivas de pluviometer_read().
 *
 * Diagrama de sequência simplificado:
 *
 *   [main — inicialização] pluviometer_init()
 *              │
 *              └─ zera tip_count, last_tip_tick, tip_snapshot, last_result
 *
 *   [ISR EXTI PA1] pluviometer_tip_callback()
 *              │
 *              ├─ HAL_GetTick() - last_tip_tick >= PLUV_DEBOUNCE_MS ?
 *              │     sim → tip_count++; last_tip_tick = agora
 *              └─     não → basculada ignorada (bounce)
 *
 *   [main — a cada ciclo de medição] pluviometer_read(&result)
 *              │
 *              ├─ seção crítica: frozen = tip_count
 *              ├─ rain_mm   = frozen × PLUV_MM_PER_TIP
 *              ├─ delta_tips = frozen - tip_snapshot
 *              ├─ delta_mm  = delta_tips × PLUV_MM_PER_TIP
 *              ├─ tip_snapshot = frozen          (atualiza base do delta)
 *              └─ result->valid = 1
 *
 *   [main — periodicamente, ex: meia-noite] pluviometer_reset()
 *              │
 *              └─ seção crítica: tip_count = 0; tip_snapshot = 0
 *
 * @author  TCC — Estação Meteorológica Automática IoT
 * @version 1.0.0
 */

/* -------------------------------------------------------------------------- */
/* Includes                                                                    */
/* -------------------------------------------------------------------------- */
#include "pluviometer.h"

/* -------------------------------------------------------------------------- */
/* Variáveis internas (file-scope)                                             */
/* -------------------------------------------------------------------------- */

/**
 * @brief Totalizador de basculadas válidas desde o último reset.
 *        Incrementado exclusivamente na ISR do EXTI (após debounce).
 *        Volatile: escrito pela ISR, lido pelo main em seção crítica.
 */
static volatile uint32_t tip_count = 0u;
static volatile uint32_t rejected_tips = 0u;
static volatile uint8_t counter_overflow = 0u;

/**
 * @brief Timestamp (HAL_GetTick) da última basculada considerada válida.
 *        Usado para debounce por software na ISR do EXTI.
 *        Volatile: lido e escrito pela ISR.
 */
static volatile uint32_t last_tip_tick = 0u;

/**
 * @brief Snapshot do totalizador no momento da última chamada a pluviometer_read().
 *        Permite calcular o delta (chuva do intervalo) sem zerar o acumulado total.
 *        Acessado apenas no contexto do main — não precisa de volatile.
 */
static uint32_t tip_snapshot = 0u;

/**
 * @brief Cache do último resultado calculado, retornado por pluviometer_get_last().
 *        Inicializado com zeros (valid = 0) em tempo de compilação.
 */
static Pluviometer_Data_t last_result = {
    .rain_mm     = 0.0f,
    .delta_mm    = 0.0f,
    .tip_count   = 0u,
    .delta_tips  = 0u,
    .valid       = 0u
};

/* -------------------------------------------------------------------------- */
/* Implementação das funções públicas                                          */
/* -------------------------------------------------------------------------- */

/**
 * @brief  Inicializa as variáveis internas do módulo pluviômetro.
 *
 * @details Garante estado inicial limpo e definido. Deve ser chamada uma única
 *          vez após MX_GPIO_Init(), antes do loop principal. Não habilita nem
 *          desabilita a interrupção EXTI — isso é gerenciado pelo CubeMX.
 */
void pluviometer_init(void)
{
    /* Seção crítica para zerar variáveis compartilhadas com a ISR.
     * Embora a ISR ainda não deva estar ativa neste ponto, a seção crítica
     * é mantida como boa prática e defesa contra reordenação de instruções. */
    uint32_t irq_state = __get_PRIMASK();
    __disable_irq();
    counter_overflow = 0u;
    rejected_tips = 0u;
    tip_count     = 0u;
    last_tip_tick = 0u;
    __set_PRIMASK(irq_state);

    tip_snapshot = 0u;

    last_result.rain_mm    = 0.0f;
    last_result.delta_mm   = 0.0f;
    last_result.tip_count  = 0u;
    last_result.delta_tips = 0u;
    last_result.valid      = 0u;
}

/**
 * @brief  Lê o totalizador de basculadas e calcula a precipitação acumulada.
 *
 * @details Captura atomicamente tip_count em uma variável local (frozen_tips)
 *          dentro de uma seção crítica, evitando race condition com a ISR do EXTI.
 *          Em seguida, realiza os cálculos de ponto flutuante no contexto do main.
 *          O campo delta_mm reflete apenas a precipitação desde a chamada anterior.
 *          Chuva acumulada zero (tip_count == 0) é resultado VÁLIDO.
 *
 * @param  result  Ponteiro para a struct de saída; comportamento indefinido se NULL.
 */
void pluviometer_read(Pluviometer_Data_t *result)
{
    if (result == NULL) return;
    uint8_t overflow;
    uint32_t frozen_tips;
    uint32_t delta_tips;

    /* Captura atômica do totalizador — seção crítica mínima (apenas a cópia) */
    uint32_t irq_state = __get_PRIMASK();
    __disable_irq();
    frozen_tips = tip_count;
    overflow = counter_overflow;
    __set_PRIMASK(irq_state);

    /* O totalizador satura ao exceder uint32_t; overflow invalida a leitura. */
    delta_tips = frozen_tips - tip_snapshot;

    /* Cálculo de ponto flutuante no contexto do main, nunca em ISR */
    result->rain_mm    = (float)frozen_tips * PLUV_MM_PER_TIP;
    result->delta_mm   = (float)delta_tips  * PLUV_MM_PER_TIP;
    result->tip_count  = frozen_tips;
    result->delta_tips = delta_tips;
    result->valid      = !overflow;

    /* Atualiza snapshot para o próximo cálculo de delta */
    tip_snapshot = frozen_tips;

    /* Atualiza o cache interno para pluviometer_get_last() */
    if (result->valid) last_result = *result;
}

/**
 * @brief  Callback de contagem de basculadas — deve ser chamado pela ISR do EXTI.
 *
 * @details Aplica debounce por software comparando HAL_GetTick() com last_tip_tick.
 *          Basculadas com intervalo inferior a PLUV_DEBOUNCE_MS são descartadas.
 *          200 ms de debounce é seguro até para chuvas muito intensas (~300 mm/h),
 *          que geram no máximo ~1 basculada/s com funil padrão (0,2794 mm/tip).
 *          NÃO realiza nenhum cálculo de ponto flutuante.
 */
void pluviometer_tip_callback(void)
{

	uint32_t now = HAL_GetTick();   /* Leitura atômica do SysTick [ms] */

    /* Debounce por software: ignora basculadas mecânicas múltiplas (bounce).
     * Subtração de uint32_t sem sinal é segura com rollover do HAL_GetTick()
     * (comportamento bem definido por aritmética modular em C99). */
    if ((now - last_tip_tick) >= (uint32_t)PLUV_DEBOUNCE_MS)
    {
        if (tip_count < UINT32_MAX) tip_count++;
        else counter_overflow = 1u;
        last_tip_tick = now;
    }
    else if (rejected_tips < UINT32_MAX) rejected_tips++;
}

/**
 * @brief  Zera o totalizador de basculadas e o snapshot de delta.
 *
 * @details Deve ser chamada periodicamente para reiniciar a contagem acumulada
 *          (ex: pelo RTC a cada 24 h, ou no início de cada ciclo de monitoramento).
 *          Usa seção crítica para garantir atomicidade com a ISR do EXTI.
 *          O cache last_result NÃO é zerado — pluviometer_get_last() continuará
 *          retornando o último valor calculado até a próxima chamada de pluviometer_read().
 */
void pluviometer_reset(void)
{
    uint32_t irq_state = __get_PRIMASK();
    __disable_irq();
    tip_count = 0u;
    counter_overflow = 0u;
    __set_PRIMASK(irq_state);

    /* Reseta o snapshot junto com o totalizador para que o próximo delta
     * seja calculado corretamente a partir do zero */
    tip_snapshot = 0u;
}

/**
 * @brief  Retorna o último resultado calculado por pluviometer_read().
 *
 * @details Retorna cópia por valor da struct interna. Se pluviometer_read()
 *          ainda não foi chamada desde a inicialização, retorna struct zerada
 *          com valid = 0.
 *
 * @return  Última Pluviometer_Data_t calculada, ou struct zerada (valid = 0).
 */
Pluviometer_Data_t pluviometer_get_last(void)
{
    return last_result;
}

/**
 * @brief  Retorna o número bruto de basculadas acumuladas desde o último reset.
 *
 * @details Captura atômica do totalizador. Útil para inspeção rápida sem
 *          preencher a struct completa. NÃO atualiza o snapshot de delta e
 *          NÃO afeta nenhum estado interno.
 *
 * @return  Valor atual de tip_count.
 */
uint32_t pluviometer_get_tip_count(void)
{
    uint32_t snapshot;

    uint32_t irq_state = __get_PRIMASK();
    __disable_irq();
    snapshot = tip_count;
    __set_PRIMASK(irq_state);

    return snapshot;
}

void pluviometer_get_diagnostics(uint32_t *rejected, uint32_t *last_tip_ms)
{
    uint32_t irq_state = __get_PRIMASK();
    __disable_irq();
    if (rejected) *rejected = rejected_tips;
    if (last_tip_ms) *last_tip_ms = last_tip_tick;
    __set_PRIMASK(irq_state);
}
