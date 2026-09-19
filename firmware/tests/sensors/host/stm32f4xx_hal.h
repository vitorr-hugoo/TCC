#ifndef FAKE_HAL_H
#define FAKE_HAL_H
#include <stdint.h>
#include <stddef.h>
typedef enum
{
    HAL_OK,
    HAL_ERROR,
    HAL_BUSY,
    HAL_TIMEOUT
} HAL_StatusTypeDef;
typedef struct
{
    uint32_t CR1, CNT, DIER, SR;
} TIM_TypeDef;
typedef struct
{
    TIM_TypeDef *Instance;
} TIM_HandleTypeDef;
extern TIM_TypeDef fake_tim6;
#define TIM6          (&fake_tim6)
#define TIM6_DAC_IRQn 54
void HAL_NVIC_ClearPendingIRQ(int irq);
typedef struct
{
    int unused;
} I2C_HandleTypeDef;
typedef struct
{
    int unused;
} GPIO_TypeDef;
typedef struct
{
    int unused;
} SPI_HandleTypeDef;
typedef struct
{
    uint32_t ClockPrescaler;
    uint32_t Resolution;
    uint32_t ScanConvMode;
    uint32_t ContinuousConvMode;
    uint32_t DiscontinuousConvMode;
    uint32_t ExternalTrigConvEdge;
    uint32_t ExternalTrigConv;
    uint32_t DataAlign;
    uint32_t NbrOfConversion;
    uint32_t DMAContinuousRequests;
    uint32_t EOCSelection;
} ADC_InitTypeDef;
typedef struct
{
    int *Instance;
    ADC_InitTypeDef Init;
} ADC_HandleTypeDef;
typedef struct
{
    uint32_t Channel, Rank, SamplingTime;
} ADC_ChannelConfTypeDef;
typedef struct
{
    uint32_t Pin, Mode, Pull, Speed;
} GPIO_InitTypeDef;
typedef enum
{
    GPIO_PIN_RESET,
    GPIO_PIN_SET
} GPIO_PinState;
extern uint32_t fake_irq;
extern int fake_adc_clock, fake_adc_instance;
#define ADC1                          (&fake_adc_instance)
#define ADC_CHANNEL_0                 0
#define ADC_SAMPLETIME_144CYCLES      144
#define ADC_CLOCK_SYNC_PCLK_DIV4      (4U)
#define ADC_RESOLUTION_12B            (12U)
#define ADC_EXTERNALTRIGCONVEDGE_NONE (0U)
#define ADC_SOFTWARE_START            (0U)
#define ADC_DATAALIGN_RIGHT           (0U)
#define ADC_EOC_SINGLE_CONV           (0U)
#define DISABLE                       (0U)
#define SPI_BAUDRATEPRESCALER_256     (256U)
#define SPI_BAUDRATEPRESCALER_8       (8U)
#define GPIO_MODE_OUTPUT_PP           1
#define GPIO_MODE_INPUT               0
#define GPIO_NOPULL                   0
#define GPIO_PULLUP                   1
#define GPIO_SPEED_FREQ_VERY_HIGH     3
#define TIM_CR1_CEN                   1u
#define TIM_IT_UPDATE                 1u
#define TIM_FLAG_UPDATE               1u
#define __get_PRIMASK()               (fake_irq)
#define __disable_irq()               (fake_irq = 1)
#define __set_PRIMASK(x)              (fake_irq = (x))
#define __HAL_TIM_DISABLE_IT(h, m)    ((h)->Instance->DIER &= ~(m))
#define __HAL_TIM_DISABLE(h)          ((h)->Instance->CR1 &= ~TIM_CR1_CEN)
#define __HAL_TIM_CLEAR_FLAG(h, m)    ((h)->Instance->SR &= ~(m))
#define __HAL_TIM_SET_COUNTER(h, v)   ((h)->Instance->CNT = (v))
#define __HAL_TIM_GET_COUNTER(h)      fake_timer_counter(h)
#define __HAL_RCC_ADC1_CLK_ENABLE()   (fake_adc_clock = 1)
#define __HAL_RCC_ADC1_CLK_DISABLE()  (fake_adc_clock = 0)
uint32_t HAL_GetTick(void);
void HAL_Delay(uint32_t ms);
uint32_t fake_timer_counter(TIM_HandleTypeDef *h);
HAL_StatusTypeDef HAL_TIM_Base_Start_IT(TIM_HandleTypeDef *h);
HAL_StatusTypeDef HAL_TIM_Base_Stop_IT(TIM_HandleTypeDef *h);
void HAL_GPIO_Init(GPIO_TypeDef *g, GPIO_InitTypeDef *cfg);
void HAL_GPIO_WritePin(GPIO_TypeDef *g, uint16_t p, GPIO_PinState state);
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *g, uint16_t p);
HAL_StatusTypeDef HAL_I2C_Master_Transmit(I2C_HandleTypeDef *h, uint16_t addr, uint8_t *buf,
                                          uint16_t len, uint32_t timeout);
HAL_StatusTypeDef HAL_I2C_Master_Receive(I2C_HandleTypeDef *h, uint16_t addr, uint8_t *buf,
                                         uint16_t len, uint32_t timeout);
HAL_StatusTypeDef HAL_ADC_Init(ADC_HandleTypeDef *handleAdc);
HAL_StatusTypeDef HAL_ADC_ConfigChannel(ADC_HandleTypeDef *handleAdc,
                                        ADC_ChannelConfTypeDef *configuracaoCanal);
HAL_StatusTypeDef HAL_ADC_Start(ADC_HandleTypeDef *h);
HAL_StatusTypeDef HAL_ADC_Stop(ADC_HandleTypeDef *h);
HAL_StatusTypeDef HAL_ADC_PollForConversion(ADC_HandleTypeDef *h, uint32_t timeout);
uint32_t HAL_ADC_GetValue(ADC_HandleTypeDef *h);
#endif
