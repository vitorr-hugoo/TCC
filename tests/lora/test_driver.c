/* Testa o lora_sx1276.c de producao, ligado a uma HAL simulada. */
#include "lora_sx1276.h"
#include <stdio.h>
#include <string.h>

SPI_HandleTypeDef hspi2;
GPIO_TypeDef test_gpio_b, test_gpio_c;

static uint8_t regs[128], fifo[256];
static uint32_t ticks;
static unsigned tx_requests, spi_calls, reset_pulses;
static int selected, burst, fifo_len;
static int disconnected, disconnect_on_tx, disconnect_after_done;
static int read_error, write_error, fifo_error, drop_frequency;
static int stuck_done, never_done, busy_forever;
static int header_error, wrong_length, bad_final_mode, final_irq_error;
static unsigned done_events;
static int fail_final_init_read;
static unsigned version_reads;
static unsigned standby_delay, standby_pending, standby_requests;
static int standby_stuck, standby_read_error;
static unsigned tick_step = 10u;

int test_spi_busy(void) { return busy_forever; }
uint32_t HAL_GetTick(void) { ticks += tick_step; return ticks; }
void HAL_Delay(uint32_t ms) { ticks += ms; }

void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState state)
{
    if (port == GPIOB && pin == GPIO_PIN_12) {
        selected = state == GPIO_PIN_RESET;
        if (!selected) burst = 0;
    }
    if (port == GPIOC && pin == GPIO_PIN_4 && state == GPIO_PIN_RESET)
        ++reset_pulses;
}

HAL_StatusTypeDef HAL_SPI_Transmit(SPI_HandleTypeDef *spi, uint8_t *data,
                                   uint16_t len, uint32_t timeout)
{
    (void)spi; (void)timeout;
    ++spi_calls;
    if (!selected) return HAL_ERROR;
    if (write_error) return HAL_ERROR;
    if (disconnected) return HAL_OK; /* SPI nao tem ACK do escravo. */
    if (burst) {
        if (fifo_error) return HAL_ERROR;
        memcpy(fifo, data, len);
        fifo_len = len;
        return HAL_OK;
    }
    if (len == 1u && (data[0] & 0x7fu) == SX1276_REG_FIFO) {
        if (header_error) return HAL_ERROR;
        burst = 1;
        return HAL_OK;
    }
    if (len != 2u) return HAL_ERROR;
    uint8_t addr = data[0] & 0x7fu;
    uint8_t value = data[1];
    if (addr == SX1276_REG_OP_MODE && value == SX1276_MODE_STANDBY &&
        regs[addr] == SX1276_MODE_SLEEP) {
        ++standby_requests;
        if (standby_delay || standby_stuck || standby_read_error) {
            standby_pending = standby_delay;
            return HAL_OK;
        }
    }
    if (final_irq_error && done_events && addr == SX1276_REG_IRQ_FLAGS) return HAL_ERROR;
    if (wrong_length && addr == SX1276_REG_PAYLOAD_LENGTH) return HAL_OK;
    if (drop_frequency && addr == SX1276_REG_FR_MSB) return HAL_OK;
    if (addr == SX1276_REG_IRQ_FLAGS) {
        if (!stuck_done) regs[addr] &= (uint8_t)~value;
    } else {
        regs[addr] = value;
    }
    if (addr == SX1276_REG_OP_MODE && value == SX1276_MODE_TX) {
        ++tx_requests;
        if (disconnect_on_tx) disconnected = 1;
    }
    return HAL_OK;
}

HAL_StatusTypeDef HAL_SPI_TransmitReceive(SPI_HandleTypeDef *spi, uint8_t *tx,
                                          uint8_t *rx, uint16_t len, uint32_t timeout)
{
    (void)spi; (void)timeout;
    ++spi_calls;
    if (!selected || len != 2u) return HAL_ERROR;
    if (read_error) return HAL_ERROR;
    rx[0] = 0;
    if (disconnected) { rx[1] = 0xffu; return HAL_OK; }
    uint8_t addr = tx[0] & 0x7fu;
    if (addr == SX1276_REG_OP_MODE && standby_requests) {
        if (standby_read_error) return HAL_ERROR;
        if (standby_pending && --standby_pending == 0) regs[addr] = SX1276_MODE_STANDBY;
    }
    if (addr == SX1276_REG_VERSION && ++version_reads == 2 && fail_final_init_read)
        return HAL_ERROR;
    if (addr == SX1276_REG_IRQ_FLAGS && tx_requests && !never_done &&
        (regs[SX1276_REG_OP_MODE] & 7u) == 3u) {
        regs[addr] |= SX1276_IRQ_TX_DONE;
        ++done_events;
        if (!bad_final_mode) regs[SX1276_REG_OP_MODE] = SX1276_MODE_STANDBY;
        rx[1] = regs[addr];
        if (disconnect_after_done) disconnected = 1;
        return HAL_OK;
    }
    rx[1] = regs[addr];
    return HAL_OK;
}

#define REQUIRE(condition, message) do { if (!(condition)) { \
    fprintf(stderr, "FAIL: %s\n", message); return 1; } } while (0)

int main(int argc, char **argv)
{
    if (argc != 2) return 2;
    const char *name = argv[1];
    const uint8_t payload[18] = {1, 42, 0, 0xfa, 0, 0x58, 2, 0xf5, 3,
                                 0xe8, 3, 0xfa, 0, 0xb4, 0, 0x7b, 0, 0x29};
    regs[SX1276_REG_VERSION] = SX1276_VERSION_EXPECTED;
    regs[SX1276_REG_OP_MODE] = SX1276_MODE_STANDBY;
    regs[SX1276_REG_MODEM_CONFIG1] = 0x72u;
    regs[SX1276_REG_MODEM_CONFIG2] = 0x94u;
    regs[SX1276_REG_MODEM_CONFIG3] = 0x04u;
    regs[SX1276_REG_FR_MSB] = 0xe4u; /* 915 MHz, cristal de 32 MHz */
    regs[SX1276_REG_FR_MID] = 0xc0u;
    regs[SX1276_REG_FR_LSB] = 0x00u;
    regs[SX1276_REG_PA_CONFIG] = 0xffu; /* PA_BOOST, 17 dBm */
    regs[SX1276_REG_PA_DAC] = 0x84u;
    regs[SX1276_REG_OCP] = 0x2bu;

    if (strcmp(name, "invalid_arguments") == 0) {
        REQUIRE(!SX1276_Transmit(NULL, 18), "NULL deve falhar");
        REQUIRE(!SX1276_Transmit(payload, 0), "tamanho zero deve falhar");
        REQUIRE(spi_calls == 0, "argumentos invalidos nao devem acessar SPI");
    } else if (strncmp(name, "mode_", 5) == 0) {
        SX1276_Config_t cfg = SX1276_DEFAULT_CONFIG;
        tick_step = 0; /* HAL_Delay advances the clock, not reading SysTick. */
        if (strcmp(name, "mode_delayed") == 0) standby_delay = 3;
        else if (strcmp(name, "mode_stuck") == 0) standby_stuck = 1;
        else if (strcmp(name, "mode_spi_error") == 0) standby_read_error = 1;
        else if (strcmp(name, "mode_wrap") == 0) { ticks = UINT32_MAX - 6u; standby_stuck = 1; }
        else return 2;
        uint32_t before = ticks;
        REQUIRE(SX1276_Init(&cfg) == (standby_delay != 0), "mode transition result");
        if (standby_delay) {
            REQUIRE(standby_requests == 1, "poll mode without rewriting request");
            REQUIRE(SX1276_CheckHealth(), "delayed standby must finish valid");
        } else {
            REQUIRE(strcmp(SX1276_LastError(), standby_read_error ? "SPI" : "CONFIG") == 0,
                    "retain correct failure reason");
            REQUIRE((uint32_t)(ticks - before) <= 15u, "bounded mode wait, including sleep delay");
            REQUIRE(!SX1276_Transmit(payload, sizeof payload), "failed init must block TX");
            REQUIRE(tx_requests == 0, "no TX after failed mode transition");
        }
    } else if (strncmp(name, "init_", 5) == 0) {
        SX1276_Config_t cfg = SX1276_DEFAULT_CONFIG;
        if (strcmp(name, "init_write_error") == 0) write_error = 1;
        else if (strcmp(name, "init_read_error") == 0) read_error = 1;
        else if (strcmp(name, "init_mismatch") == 0) {
            drop_frequency = 1;
            regs[SX1276_REG_FR_MSB] = 0;
        }
        else if (strcmp(name, "init_final_read_error") == 0) fail_final_init_read = 1;
        else if (strcmp(name, "init_ok") != 0) return 2;
        int expected = strcmp(name, "init_ok") == 0;
        REQUIRE(SX1276_Init(&cfg) == expected,
                "Init deve rejeitar falha SPI ou configuracao nao gravada");
        if (fail_final_init_read) {
            fail_final_init_read = 0;
            REQUIRE(!SX1276_Transmit(payload, sizeof payload), "init incompleta nao deve habilitar TX posterior");
            REQUIRE(tx_requests == 0, "init incompleta deve impedir TX");
        }
    } else {
        SX1276_Config_t cfg = SX1276_DEFAULT_CONFIG;
        if (strcmp(name, "uninitialized") != 0) {
            REQUIRE(SX1276_Init(&cfg), "precondicao: init saudavel deve passar");
            spi_calls = 0;
            ticks = 0;
        }
        int expected = 0;
        if (strcmp(name, "tx_ok") == 0) expected = 1;
        else if (strcmp(name, "radio_ff") == 0) disconnected = 1;
        else if (strcmp(name, "wrong_version") == 0) regs[SX1276_REG_VERSION] = 0;
        else if (strcmp(name, "read_error") == 0) read_error = 1;
        else if (strcmp(name, "write_error") == 0) {
            write_error = 1;
            regs[SX1276_REG_IRQ_FLAGS] = SX1276_IRQ_TX_DONE;
        }
        else if (strcmp(name, "fifo_error") == 0) fifo_error = 1;
        else if (strcmp(name, "stale_done") == 0) {
            stuck_done = 1;
            regs[SX1276_REG_IRQ_FLAGS] = SX1276_IRQ_TX_DONE;
        }
        else if (strcmp(name, "disconnect_during_tx") == 0) disconnect_on_tx = 1;
        else if (strcmp(name, "disconnect_after_done") == 0) disconnect_after_done = 1;
        else if (strcmp(name, "tx_timeout") == 0) never_done = 1;
        else if (strcmp(name, "busy_timeout") == 0) busy_forever = 1;
        else if (strcmp(name, "header_error") == 0) header_error = 1;
        else if (strcmp(name, "wrong_length") == 0) wrong_length = 1;
        else if (strcmp(name, "bad_final_mode") == 0) bad_final_mode = 1;
        else if (strcmp(name, "final_irq_error") == 0) final_irq_error = 1;
        else if (strcmp(name, "lost_config") == 0) regs[SX1276_REG_MODEM_CONFIG2] = 0;
        else if (strcmp(name, "reset_radio") == 0) regs[SX1276_REG_OP_MODE] = 1;
        else if (strcmp(name, "timeout_wrap") == 0) { never_done = 1; ticks = UINT32_MAX - 1000u; }
        else if (strcmp(name, "transient_retry") == 0) read_error = 1;
        else if (strcmp(name, "uninitialized") != 0) return 2;
        uint32_t initial_ticks = ticks;
        REQUIRE(SX1276_Transmit(payload, sizeof payload) == expected,
                "resultado da transmissao nao corresponde a falha simulada");
        REQUIRE(reset_pulses == 0, "checagem durante TX nao deve resetar o radio");
        if (expected) {
            REQUIRE(tx_requests == 1, "deve solicitar uma unica transmissao");
            REQUIRE(fifo_len == sizeof payload && memcmp(fifo, payload, sizeof payload) == 0,
                    "bytes escritos no FIFO devem ser exatamente os fornecidos");
            REQUIRE(regs[SX1276_REG_OP_MODE] == SX1276_MODE_STANDBY, "estado final deve ser standby");
        }
        if (strcmp(name, "tx_timeout") == 0 || strcmp(name, "timeout_wrap") == 0) {
            REQUIRE((uint32_t)(ticks - initial_ticks) >= 3000 && (uint32_t)(ticks - initial_ticks) <= 3800,
                    "TX deve respeitar prazo incluindo verificacoes SPI limitadas");
            REQUIRE(regs[SX1276_REG_OP_MODE] == SX1276_MODE_STANDBY, "timeout deve tentar standby");
        }
        if (strcmp(name, "radio_ff") == 0 || strcmp(name, "wrong_version") == 0 ||
            strcmp(name, "fifo_error") == 0 || strcmp(name, "stale_done") == 0)
            REQUIRE(tx_requests == 0, "falha anterior a TX deve impedir inicio da transmissao");
        if (strcmp(name, "transient_retry") == 0) {
            read_error = 0;
            REQUIRE(SX1276_Transmit(payload, sizeof payload), "falha transitoria nao deve bloquear tentativa seguinte");
        }
        if (strcmp(name, "lost_config") == 0) {
            regs[SX1276_REG_MODEM_CONFIG2] = 0x94;
            REQUIRE(!SX1276_Transmit(payload, sizeof payload), "perda de config exige init explicita");
            REQUIRE(SX1276_Init(&cfg), "init explicita deve recuperar");
            REQUIRE(SX1276_Transmit(payload, sizeof payload), "TX deve funcionar depois de init validada");
        }
    }
    REQUIRE(!selected, "NSS deve ser liberado em todos os retornos");
    puts("PASS");
    return 0;
}
