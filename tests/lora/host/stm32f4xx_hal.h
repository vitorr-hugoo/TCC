#ifndef LORA_TEST_HAL_H
#define LORA_TEST_HAL_H

/* Apenas a interface HAL usada pelo driver real. Nao vai para o firmware. */
#include <stdint.h>
#include <stddef.h>

typedef struct { unsigned unused; } SPI_HandleTypeDef;
typedef struct { unsigned unused; } GPIO_TypeDef;
typedef enum { HAL_OK, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT } HAL_StatusTypeDef;
typedef enum { GPIO_PIN_RESET, GPIO_PIN_SET } GPIO_PinState;

extern GPIO_TypeDef test_gpio_b, test_gpio_c;
#define GPIOB (&test_gpio_b)
#define GPIOC (&test_gpio_c)
#define GPIO_PIN_12 0x1000u
#define GPIO_PIN_4 0x0010u
#define SPI_FLAG_BSY 1u
#define __HAL_SPI_GET_FLAG(handle, flag) test_spi_busy()

int test_spi_busy(void);
void HAL_GPIO_WritePin(GPIO_TypeDef *, uint16_t, GPIO_PinState);
void HAL_Delay(uint32_t);
uint32_t HAL_GetTick(void);
HAL_StatusTypeDef HAL_SPI_Transmit(SPI_HandleTypeDef *, uint8_t *, uint16_t, uint32_t);
HAL_StatusTypeDef HAL_SPI_TransmitReceive(SPI_HandleTypeDef *, uint8_t *, uint8_t *, uint16_t, uint32_t);
#endif
