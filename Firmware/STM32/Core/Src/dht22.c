/**
 ******************************************************************************
 * @file    dht22.c
 * @brief   Implementação do driver DHT22 (AM2302)
 ******************************************************************************
 */

#include "dht22.h"

/* ─── Helpers internos (static) ──────────────────────────────────────────── */

/**
 * @brief  Reconfigura o pino GPIO como saída push-pull.
 */
static void _set_output(DHT22_Handle_t *hdev)
{
    GPIO_InitTypeDef cfg = {0};
    cfg.Pin   = hdev->gpio_pin;
    cfg.Mode  = GPIO_MODE_OUTPUT_PP;
    cfg.Pull  = GPIO_NOPULL;
    cfg.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(hdev->gpio_port, &cfg);
}

/**
 * @brief  Reconfigura o pino GPIO como entrada com pull-up interno.
 *         O pull-up interno serve como fallback caso o resistor externo
 *         (4,7 kΩ recomendado) esteja ausente ou seja de valor elevado.
 */
static void _set_input(DHT22_Handle_t *hdev)
{
    GPIO_InitTypeDef cfg = {0};
    cfg.Pin   = hdev->gpio_pin;
    cfg.Mode  = GPIO_MODE_INPUT;
    cfg.Pull  = GPIO_PULLUP;
    HAL_GPIO_Init(hdev->gpio_port, &cfg);
}

/**
 * @brief  Aguarda o pino atingir o estado desejado ou retorna erro por timeout.
 *
 * @param  hdev          Handle do sensor.
 * @param  expected_state  GPIO_PIN_SET ou GPIO_PIN_RESET.
 * @retval DHT22_OK | DHT22_ERR_TIMEOUT
 */
static DHT22_Status_t _wait_for(DHT22_Handle_t    *hdev,
                                GPIO_PinState      expected_state)
{
    uint16_t timeout = 0;
    while (HAL_GPIO_ReadPin(hdev->gpio_port, hdev->gpio_pin) != expected_state) {
        if (++timeout > DHT22_TIMEOUT_TICKS) {
            return DHT22_ERR_TIMEOUT;
        }
    }
    return DHT22_OK;
}

/* ─── API pública ────────────────────────────────────────────────────────── */

void DHT22_Init(DHT22_Handle_t    *hdev,
                GPIO_TypeDef      *gpio_port,
                uint16_t           gpio_pin,
                TIM_HandleTypeDef *htim)
{
    hdev->gpio_port   = gpio_port;
    hdev->gpio_pin    = gpio_pin;
    hdev->htim        = htim;
    hdev->temperature = 0.0f;
    hdev->humidity    = 0.0f;
}

DHT22_Status_t DHT22_Read(DHT22_Handle_t *hdev)
{
    uint8_t raw[5] = {0};

    /* ── 1. Sinal de START ──────────────────────────────────────────────── */
    /*
     * MCU puxa DATA para baixo por ≥ 1 ms para acordar o sensor.
     * Usamos 18 ms para garantir robustez mesmo com cabos longos.
     */
    _set_output(hdev);
    HAL_GPIO_WritePin(hdev->gpio_port, hdev->gpio_pin, GPIO_PIN_RESET);
    HAL_Delay(DHT22_START_LOW_MS);
    HAL_GPIO_WritePin(hdev->gpio_port, hdev->gpio_pin, GPIO_PIN_SET);

    /* ── 2. Muda para entrada e aguarda a resposta (acknowledge) ────────── */
    /*
     * Sequência de acknowledge do DHT22:
     *   a) Sensor puxa LOW por ~80 µs
     *   b) Sensor solta HIGH por ~80 µs
     *   c) Começa a transmitir os 40 bits de dados
     */
    _set_input(hdev);

    /* Aguarda o sensor assumir o barramento (queda para LOW) */
    if (_wait_for(hdev, GPIO_PIN_RESET) != DHT22_OK) return DHT22_ERR_TIMEOUT;
    /* Aguarda subida do acknowledge */
    if (_wait_for(hdev, GPIO_PIN_SET)   != DHT22_OK) return DHT22_ERR_TIMEOUT;
    /* Aguarda fim do pulso alto de acknowledge (início real dos dados) */
    if (_wait_for(hdev, GPIO_PIN_RESET) != DHT22_OK) return DHT22_ERR_TIMEOUT;

    /* ── 3. Leitura dos 40 bits (5 bytes) ──────────────────────────────── */
    /*
     * Protocolo de cada bit:
     *  - Pulso LOW de ~50 µs  (início do bit)
     *  - Pulso HIGH:
     *      ~26–28 µs → bit '0'
     *      ~70    µs → bit '1'
     *
     * Estratégia: zera o timer ao detectar a subida do pino,
     * lê o contador ao detectar a descida e compara com o limiar.
     */
    for (int byte_idx = 0; byte_idx < 5; byte_idx++) {
        for (int bit_idx = 7; bit_idx >= 0; bit_idx--) {

            /* Aguarda o pulso LOW (separador entre bits) terminar */
            if (_wait_for(hdev, GPIO_PIN_SET) != DHT22_OK) return DHT22_ERR_TIMEOUT;

            /* Zera o contador e mede a duração do pulso HIGH */
            __HAL_TIM_SET_COUNTER(hdev->htim, 0);

            /* Aguarda a descida (fim do pulso de dados) */
            if (_wait_for(hdev, GPIO_PIN_RESET) != DHT22_OK) return DHT22_ERR_TIMEOUT;

            /* Classifica o bit pelo tempo medido */
            if (__HAL_TIM_GET_COUNTER(hdev->htim) > DHT22_BIT_THRESHOLD_US) {
                raw[byte_idx] |= (1 << bit_idx); /* bit '1' */
            }
            /* bit '0': raw[byte_idx] já está zerado, não precisa escrever */
        }
    }

    /* ── 4. Verificação de checksum ─────────────────────────────────────── */
    /*
     * O 5º byte deve ser igual aos 4 bytes de dados somados (módulo 256).
     *   raw[0] = MSB umidade
     *   raw[1] = LSB umidade
     *   raw[2] = MSB temperatura
     *   raw[3] = LSB temperatura
     *   raw[4] = checksum
     */
    uint8_t checksum = (uint8_t)(raw[0] + raw[1] + raw[2] + raw[3]);
    if (raw[4] != checksum) {
        return DHT22_ERR_CHECKSUM;
    }

    /* ── 5. Conversão dos dados brutos ─────────────────────────────────── */
    /*
     * Umidade e temperatura são inteiros de 16 bits com 1 casa decimal:
     *   Valor real = word / 10.0
     *
     * Para temperatura: o bit 15 (MSB do raw[2]) indica sinal negativo.
     *   Se bit 15 = 1  →  temperatura negativa
     */
    hdev->humidity = (float)((raw[0] << 8) | raw[1]) / 10.0f;

    uint16_t temp_raw = (uint16_t)((raw[2] << 8) | raw[3]);
    if (temp_raw & 0x8000) {
        /* Temperatura negativa: limpa o bit de sinal e inverte */
        hdev->temperature = -((float)(temp_raw & 0x7FFF) / 10.0f);
    } else {
        hdev->temperature = (float)temp_raw / 10.0f;
    }

    return DHT22_OK;
}

const char *DHT22_StatusStr(DHT22_Status_t status)
{
    switch (status) {
        case DHT22_OK:           return "OK";
        case DHT22_ERR_TIMEOUT:  return "TIMEOUT - Verifique fiacao e pull-up";
        case DHT22_ERR_CHECKSUM: return "CHECKSUM - Ruido no cabo ou sensor com defeito";
        default:                 return "ERRO DESCONHECIDO";
    }
}
