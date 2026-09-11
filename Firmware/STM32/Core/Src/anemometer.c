/**
 * @file    anemometer.c
 * @brief   Implementação do módulo de anemômetro de conchas (WH-SP-WS01).
 *
 * @details Contagem de pulsos via EXTI (PC7 / borda de descida) com janela de
 *          integração de 1 segundo controlada pelo TIM6. O resultado é calculado
 *          no loop principal após sinalização por flag — nenhum cálculo de ponto
 *          flutuante é realizado dentro das ISRs.
 *
 * Diagrama de sequência simplificado:
 *
 *   [main] anemometer_start_sampling()
 *              │
 *              ├─ zera pulse_count, last_pulse_tick, result_ready
 *              └─ HAL_TIM_Base_Start_IT(htim6)  → TIM6 contando 1 s
 *
 *   [ISR EXTI PC7] anemometer_pulse_callback()
 *              │
 *              ├─ HAL_GetTick() - last_pulse_tick >= ANEM_DEBOUNCE_MS ?
 *              │     sim → pulse_count++; last_pulse_tick = agora
 *              └─     não → pulso ignorado (bounce)
 *
 *   [ISR TIM6 overflow] anemometer_stop_sampling()
 *              │
 *              ├─ HAL_TIM_Base_Stop_IT(htim6)
 *              ├─ __disable_irq() → frozen_count = pulse_count; pulse_count = 0 → __enable_irq()
 *              └─ result_ready = 1
 *
 *   [main] anemometer_is_ready() == 1
 *              │
 *              └─ anemometer_calculate(ANEM_SAMPLE_WINDOW_MS, &wind)
 *                     → preenche speed_ms, speed_kmh, pulse_count, valid = 1
 *
 * @author  TCC — Estação Meteorológica Automática IoT
 * @version 1.0.0
 */

/* -------------------------------------------------------------------------- */
/* Includes                                                                    */
/* -------------------------------------------------------------------------- */
#include "anemometer.h"

/* -------------------------------------------------------------------------- */
/* Variáveis internas (file-scope)                                             */
/* -------------------------------------------------------------------------- */
static volatile uint8_t sampling_active = 0u;
static volatile uint32_t sampling_start_tick = 0u;
/**
 * @brief Contador de pulsos do reed switch incrementado na ISR do EXTI.
 *        Declarado volatile porque é escrito pela ISR e lido pela ISR do TIM6.
 */
static volatile uint32_t pulse_count = 0u;

/**
 * @brief Timestamp (HAL_GetTick) do último pulso considerado válido.
 *        Usado para implementar o debounce por software na ISR do EXTI.
 *        Volatile porque é lido e escrito pela ISR do EXTI.
 */
static volatile uint32_t last_pulse_tick = 0u;

/**
 * @brief Flag que sinaliza ao loop principal que uma janela de 1 s foi
 *        encerrada e o resultado está disponível para cálculo.
 *        Volatile: escrito pela ISR do TIM6, lido pelo main.
 */
static volatile uint8_t result_ready = 0u;

/**
 * @brief Cópia atômica de pulse_count capturada ao final da janela.
 *        Acessada apenas pelo contexto do main (após seção crítica na ISR).
 *        Não precisa de volatile: só é escrito dentro de seção crítica (__disable_irq)
 *        e lido no main após checar result_ready (que fornece a barreira).
 */
static uint32_t frozen_count = 0u;

/**
 * @brief Cache do último resultado calculado, retornado por anemometer_get_last().
 *        Inicializado com zeros (valid = 0) em tempo de compilação.
 */
static Anemometer_Data_t last_result = {
    .speed_ms    = 0.0f,
    .speed_kmh   = 0.0f,
    .pulse_count = 0u,
    .valid       = 0u
};

/* -------------------------------------------------------------------------- */
/* Implementação das funções públicas                                          */
/* -------------------------------------------------------------------------- */

/**
 * @brief  Inicializa as variáveis internas do módulo anemômetro.
 *
 * @details Garante estado inicial limpo e definido. Deve ser chamada uma única
 *          vez após MX_GPIO_Init() e MX_TIM6_Init(), antes do loop principal.
 *          Não inicia o timer nem habilita interrupções.
 */
void anemometer_init(void)
{
    pulse_count     = 0u;
    last_pulse_tick = 0u;
    result_ready    = 0u;
    frozen_count    = 0u;
    sampling_active     = 0u;
    sampling_start_tick = 0u;

    last_result.speed_ms    = 0.0f;
    last_result.speed_kmh   = 0.0f;
    last_result.pulse_count = 0u;
    last_result.valid       = 0u;

}

/**
 * @brief  Inicia uma nova janela de amostragem de 1 segundo.
 *
 * @details Zera o estado interno da janela atual e inicia o TIM6 em modo
 *          interrupção. O contador de pulsos começa a ser incrementado
 *          imediatamente via ISR do EXTI.
 *
 * @param  htim  Handle do TIM6, passado de main.c (&htim6).
 */
void anemometer_start_sampling(TIM_HandleTypeDef *htim)
{
    /* Zera o estado da janela anterior antes de iniciar o timer para evitar
     * que um pulso chegue entre o zero e o Start e seja atribuído à janela errada.
     * A ordem importa: primeiro zera, depois inicia. */
    __disable_irq();
    pulse_count     = 0u;
    last_pulse_tick = 0u;
    result_ready    = 0u;
    sampling_active = 1u;
    sampling_start_tick = HAL_GetTick();
    __enable_irq();

    /* Inicia a contagem de 1 s (TIM6 configurado com ARR=999, PSC=89999 → 1 kHz) */
    HAL_TIM_Base_Start_IT(htim);
}

/**
 * @brief  Encerra a janela de amostragem e congela o contador de pulsos.
 *
 * @details Chamada exclusivamente dentro de HAL_TIM_PeriodElapsedCallback
 *          quando htim->Instance == TIM6. Para o timer, captura atomicamente
 *          pulse_count em frozen_count e sinaliza resultado disponível.
 *          NÃO realiza nenhum cálculo de ponto flutuante.
 *
 * @param  htim  Handle do TIM6 recebido pelo callback.
 */
void anemometer_stop_sampling(TIM_HandleTypeDef *htim)
{
    /* Para o timer para encerrar exatamente a janela de 1 s */
    HAL_TIM_Base_Stop_IT(htim);

    /* Seção crítica: copia pulse_count para frozen_count atomicamente.
     * __disable_irq / __enable_irq é o padrão sem FreeRTOS no STM32 HAL.
     * Nota: esta função já está dentro de uma ISR (TIM6), portanto a ISR
     * do EXTI (mesma prioridade ou inferior) não pode preemptá-la neste ponto.
     * A seção crítica é mantida por clareza e para cobrir casos de configuração
     * de prioridade diferentes no futuro. */
    __disable_irq();
    frozen_count = pulse_count;
    pulse_count  = 0u;           /* Reseta para próxima janela */
    __enable_irq();

    /* Sinaliza ao loop principal que o resultado está disponível */
    result_ready = 1u;
    sampling_active = 0u;
}

/**
 * @brief  Calcula a velocidade do vento a partir dos pulsos congelados.
 *
 * @details Aplica a fórmula:
 *              f_Hz    = frozen_count / (window_ms / 1000.0f)
 *              v_kmh   = f_Hz × ANEM_KMH_PER_HZ
 *              v_ms    = v_kmh / 3.6f
 *          Resultado de calmaria (frozen_count == 0) é válido: velocidade = 0.
 *          Se result_ready == 0, a função retorna sem modificar nada.
 *          Consome o flag result_ready após processamento bem-sucedido.
 *
 * @param  window_ms  Duração da janela [ms] — use ANEM_SAMPLE_WINDOW_MS (1000).
 * @param  result     Ponteiro para a struct de saída; comportamento indefinido se NULL.
 */
void anemometer_calculate(uint32_t window_ms, Anemometer_Data_t *result)
{
    float f_hz;
    float v_kmh;
    float v_ms;

    /* Guarda: só processa se uma janela foi de fato concluída */
    if (result_ready == 0u)
    {
        return;
    }

    /* Evita divisão por zero (janela de 0 ms nunca deveria ocorrer, mas defensivo) */
    if (window_ms == 0u)
    {
        return;
    }

    /* Cálculo de velocidade — realizado no contexto do main, nunca em ISR */
    f_hz  = (float)frozen_count / ((float)window_ms / 1000.0f);
    v_kmh = f_hz * ANEM_KMH_PER_HZ;
    v_ms  = v_kmh / 3.6f;

    /* Preenche a struct de saída */
    result->speed_ms    = v_ms;
    result->speed_kmh   = v_kmh;
    result->pulse_count = frozen_count;
    result->valid       = 1u;

    /* Atualiza o cache interno para anemometer_get_last() */
    last_result = *result;

    /* Consome o flag: próxima chamada a is_ready() retornará 0
     * até que uma nova janela seja concluída */
    result_ready = 0u;
}

/**
 * @brief  Callback de contagem de pulsos — deve ser chamado pela ISR do EXTI.
 *
 * @details Aplica debounce por software: compara o tick atual com last_pulse_tick.
 *          Pulsos com intervalo inferior a ANEM_DEBOUNCE_MS são descartados.
 *          Pulsos válidos incrementam pulse_count.
 *          NÃO realiza nenhum cálculo de ponto flutuante.
 */
void anemometer_pulse_callback(void)
{
	if (sampling_active == 0u)
	{
	  return;
	}

	uint32_t now = HAL_GetTick();   /* Leitura atômica do SysTick [ms] */

    /* Debounce por software: ignora pulsos muito próximos do anterior.
     * A subtração (now - last_pulse_tick) é segura mesmo com rollover de uint32_t
     * (comportamento bem definido em aritmética modular de inteiros sem sinal em C99). */
	if ((now - sampling_start_tick) < 100u)
	    {
	        return;
	    }

	if ((now - last_pulse_tick) >= (uint32_t)ANEM_DEBOUNCE_MS)
    {
        pulse_count++;
        last_pulse_tick = now;
    }
    /* else: pulso descartado (bounce residual) */
}

/**
 * @brief  Verifica se uma janela de amostragem foi concluída.
 *
 * @details Lê o flag result_ready sem consumi-lo. Apenas anemometer_calculate()
 *          limpa o flag após processar os dados.
 *
 * @return  1  se resultado disponível; 0 caso contrário.
 */
uint8_t anemometer_is_ready(void)
{
    return result_ready;
}

/**
 * @brief  Retorna o último resultado calculado por anemometer_calculate().
 *
 * @details Retorna cópia por valor da struct interna. Se nenhuma janela foi
 *          concluída e calculada ainda, valid == 0 e todos os campos são zero.
 *
 * @return  Última Anemometer_Data_t válida, ou struct zerada (valid = 0).
 */
Anemometer_Data_t anemometer_get_last(void)
{
    return last_result;
}
