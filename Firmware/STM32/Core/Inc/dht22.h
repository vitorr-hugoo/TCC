/**
 ******************************************************************************
 * @file    dht22.h
 * @brief   Driver para o sensor de temperatura e umidade DHT22 (AM2302)
 *          Compatível com STM32 HAL (série F4).
 *
 * @details Esta biblioteca implementa o protocolo single-wire proprietário
 *          do DHT22. O tempo de cada bit é medido via hardware timer (qualquer
 *          TIM de uso geral), eliminando dependência de delays de software
 *          e tornando a leitura mais robusta contra jitter de interrupções.
 *
 * Como usar:
 *  1. Configure um TIM com prescaler para resolução de ~1 µs por tick.
 *     Exemplo para SYSCLK = 150 MHz com APB2 sem divisor:
 *       Prescaler = 150 - 1  →  1 tick = 1 µs
 *  2. Inicie o timer no main antes de usar a lib:
 *       HAL_TIM_Base_Start(&htim10);
 *  3. Declare o handle e inicialize:
 *       DHT22_Handle_t dht;
 *       DHT22_Init(&dht, GPIOA, GPIO_PIN_8, &htim10);
 *  4. Leia o sensor:
 *       DHT22_Status_t status = DHT22_Read(&dht);
 *       if (status == DHT22_OK) { float t = dht.temperature; }
 ******************************************************************************
 */

#ifndef DHT22_H
#define DHT22_H

#ifdef __cplusplus
extern "C" {
#endif

/* ─── Includes ──────────────────────────────────────────────────────────── */
#include "stm32f4xx_hal.h"
#include <stdint.h>

/* ─── Constantes de temporização (µs) ───────────────────────────────────── */
/** Duração do sinal de START em nível baixo (spec: mínimo 1 ms) */
#define DHT22_START_LOW_MS       18u

/** Tempo limite de espera por qualquer transição de borda (ticks de timer) */
#define DHT22_TIMEOUT_TICKS      1000u

/**
 * Limiar de duração do pulso alto para distinguir bit '0' de bit '1'.
 * bit '0': pulso alto ~26 µs
 * bit '1': pulso alto ~70 µs
 * Limiar recomendado: ~40 µs  →  40 ticks com resolução de 1 µs/tick
 */
#define DHT22_BIT_THRESHOLD_US   40u

/* ─── Tipo de retorno ────────────────────────────────────────────────────── */
/**
 * @brief Códigos de status retornados por DHT22_Read().
 */
typedef enum {
    DHT22_OK            = 0x00, /**< Leitura bem-sucedida                  */
    DHT22_ERR_TIMEOUT   = 0x01, /**< Timeout aguardando resposta do sensor */
    DHT22_ERR_CHECKSUM  = 0x02, /**< Dados recebidos com erro de paridade  */
} DHT22_Status_t;

/* ─── Handle do sensor ───────────────────────────────────────────────────── */
/**
 * @brief Estrutura de controle de uma instância do sensor DHT22.
 *        Instancie uma struct por sensor utilizado.
 */
typedef struct {
    /* ── Configuração (preenchida por DHT22_Init) ── */
    GPIO_TypeDef        *gpio_port;   /**< Porta GPIO do pino de dados       */
    uint16_t             gpio_pin;    /**< Pino GPIO (ex.: GPIO_PIN_8)       */
    TIM_HandleTypeDef   *htim;        /**< Timer usado para medir duração    */

    /* ── Dados lidos (válidos quando DHT22_Read retorna DHT22_OK) ── */
    float                temperature; /**< Temperatura em graus Celsius      */
    float                humidity;    /**< Umidade relativa em %             */
} DHT22_Handle_t;

/* ─── Protótipos ─────────────────────────────────────────────────────────── */

/**
 * @brief  Inicializa o handle do DHT22 com os recursos de hardware.
 *
 * @param  hdev       Ponteiro para o handle do sensor.
 * @param  gpio_port  Porta GPIO conectada ao pino DATA do sensor.
 * @param  gpio_pin   Pino GPIO (ex.: GPIO_PIN_8).
 * @param  htim       Ponteiro para o handle do timer configurado a ~1 µs/tick.
 *                    O timer deve estar iniciado (HAL_TIM_Base_Start) antes
 *                    de chamar DHT22_Read.
 */
void DHT22_Init(DHT22_Handle_t    *hdev,
                GPIO_TypeDef      *gpio_port,
                uint16_t           gpio_pin,
                TIM_HandleTypeDef *htim);

/**
 * @brief  Realiza uma leitura completa do sensor DHT22 (40 bits).
 *
 * @note   O DHT22 requer intervalo mínimo de 2 s entre leituras consecutivas.
 *         O controle desse intervalo é responsabilidade do código chamador.
 *
 * @param  hdev  Ponteiro para o handle inicializado.
 * @retval DHT22_OK           Dados válidos em hdev->temperature e hdev->humidity.
 * @retval DHT22_ERR_TIMEOUT  Sensor não respondeu (verificar fiação/pull-up).
 * @retval DHT22_ERR_CHECKSUM Dado corrompido (ruído no cabo ou sensor defeituoso).
 */
DHT22_Status_t DHT22_Read(DHT22_Handle_t *hdev);

/**
 * @brief  Retorna uma string descritiva do código de erro.
 *
 * @param  status  Código retornado por DHT22_Read().
 * @retval Ponteiro para string estática (não modificar).
 */
const char *DHT22_StatusStr(DHT22_Status_t status);


#ifdef __cplusplus
}
#endif

#endif /* DHT22_H */
