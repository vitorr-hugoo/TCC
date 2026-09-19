/**
 * @file anemometer.c
 * @brief Implementa a contagem de pulsos e a janela de amostragem do anemômetro.
 */

/* Window state is shared with TIM6/EXTI. Preserve the caller's IRQ mask. */
#include "anemometer.h"
#include <string.h>
static volatile uint32_t pulse_count, rejected_count, last_pulse_tick, sampling_start_tick;
static volatile uint8_t sampling_active, result_ready;
static volatile HAL_StatusTypeDef window_status = HAL_ERROR;
static uint32_t frozen_count;
static Anemometer_Data_t last_result;

static void timer_off(TIM_HandleTypeDef *htim)
{
    __HAL_TIM_DISABLE_IT(htim, TIM_IT_UPDATE);
    __HAL_TIM_DISABLE(htim);
    __HAL_TIM_CLEAR_FLAG(htim, TIM_FLAG_UPDATE);
    __HAL_TIM_SET_COUNTER(htim, 0);
    if (htim->Instance == TIM6)
    {
        HAL_NVIC_ClearPendingIRQ(TIM6_DAC_IRQn);
    }
}
void anemometer_init(void)
{
    uint32_t irq = __get_PRIMASK();
    __disable_irq();
    pulse_count = rejected_count = last_pulse_tick = sampling_start_tick = frozen_count = 0;
    sampling_active = result_ready = 0;
    window_status = HAL_ERROR;
    memset(&last_result, 0, sizeof(last_result));
    __set_PRIMASK(irq);
}
HAL_StatusTypeDef anemometer_start_sampling(TIM_HandleTypeDef *htim)
{
    if (!htim)
    {
        return HAL_ERROR;
    }
    uint32_t irq = __get_PRIMASK();
    __disable_irq();
    timer_off(htim);
    pulse_count = rejected_count = frozen_count = 0;
    result_ready = 0;
    sampling_start_tick = HAL_GetTick();
    sampling_active = 1;
    HAL_StatusTypeDef result = HAL_TIM_Base_Start_IT(htim);
    if (result != HAL_OK)
    {
        sampling_active = 0;
        timer_off(htim);
        window_status = result == HAL_BUSY ? HAL_ERROR : result;
    }
    else
    {
        window_status = HAL_BUSY;
    }
    __set_PRIMASK(irq);
    return result;
}
void anemometer_stop_sampling(TIM_HandleTypeDef *htim)
{
    uint32_t irq = __get_PRIMASK();
    __disable_irq();
    uint32_t elapsed = HAL_GetTick() - sampling_start_tick;
    /* Ignore late IRQs from a cancelled/previous window. */
    /* TIM6 and SysTick can cross the boundary in either order (1 ms). */
    if (sampling_active && elapsed >= ANEM_SAMPLE_WINDOW_MS - 1u)
    {
        window_status = HAL_TIM_Base_Stop_IT(htim);
        timer_off(htim);
        if (elapsed >= ANEM_SAMPLE_WINDOW_MS + ANEM_TIMEOUT_MARGIN_MS)
        {
            window_status = HAL_TIMEOUT;
        }
        frozen_count = pulse_count;
        sampling_active = 0;
        result_ready = window_status == HAL_OK;
    }
    __set_PRIMASK(irq);
}
HAL_StatusTypeDef anemometer_service(TIM_HandleTypeDef *htim, uint32_t now)
{
    uint32_t irq = __get_PRIMASK();
    __disable_irq();
    if (sampling_active &&
        (uint32_t)(now - sampling_start_tick) >= ANEM_SAMPLE_WINDOW_MS + ANEM_TIMEOUT_MARGIN_MS)
    {
        (void)HAL_TIM_Base_Stop_IT(htim);
        timer_off(htim);
        sampling_active = result_ready = 0;
        window_status = HAL_TIMEOUT;
    }
    HAL_StatusTypeDef result = window_status;
    if (!sampling_active && result == HAL_BUSY)
    {
        result = HAL_ERROR;
    }
    __set_PRIMASK(irq);
    return result;
}
void anemometer_calculate(uint32_t window_ms, Anemometer_Data_t *result)
{
    if (!result)
    {
        return;
    }
    memset(result, 0, sizeof(*result));
    if (!result_ready || !window_ms || window_status != HAL_OK)
    {
        return;
    }
    result->pulse_count = frozen_count;
    result->speed_kmh = (float)frozen_count * 1000.0f / window_ms * ANEM_KMH_PER_HZ;
    result->speed_ms = result->speed_kmh / 3.6f;
    result->valid = 1;
    last_result = *result;
    result_ready = 0;
}
void anemometer_pulse_callback(void)
{
    if (!sampling_active)
    {
        return;
    }
    uint32_t now = HAL_GetTick();
    /* Preserve the existing startup filter and debounce calibration. */
    if ((uint32_t)(now - sampling_start_tick) < 100u ||
        (pulse_count && (uint32_t)(now - last_pulse_tick) < ANEM_DEBOUNCE_MS))
    {
        if (rejected_count < UINT32_MAX)
        {
            rejected_count++;
        }
        return;
    }
    if (pulse_count < UINT32_MAX)
    {
        pulse_count++;
    }
    last_pulse_tick = now;
}
uint8_t anemometer_is_ready(void)
{
    return result_ready;
}
Anemometer_Data_t anemometer_get_last(void)
{
    return last_result;
}
void anemometer_get_diagnostics(uint32_t *accepted, uint32_t *rejected, uint32_t *last_pulse_ms)
{
    uint32_t irq = __get_PRIMASK();
    __disable_irq();
    if (accepted)
    {
        *accepted = pulse_count;
    }
    if (rejected)
    {
        *rejected = rejected_count;
    }
    if (last_pulse_ms)
    {
        *last_pulse_ms = last_pulse_tick;
    }
    __set_PRIMASK(irq);
}
