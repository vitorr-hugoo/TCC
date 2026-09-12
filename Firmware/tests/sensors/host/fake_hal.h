#include "stm32f4xx_hal.h"
extern uint32_t fake_ms, fake_us, fake_delay;
extern HAL_StatusTypeDef timer_start, timer_stop, i2c_result, adc_start, adc_stop, adc_poll,
    adc_config;
extern HAL_StatusTypeDef adcInicializacao;
extern uint8_t bmp_regs[256], dht_raw[5];
extern uint16_t bh_raw, adc_values[16];
extern unsigned bh_commands, adc_index, bmp_busy_reads, bmp_reset_count;
extern unsigned chamadasAdcConfig;
extern int dht_stuck, timer_stuck, bad_pulse, corrupt_config, fail_after_data;
extern int fail_receive;
extern unsigned dht_counter_reads;
void fake_reset(void);
void fake_bmp_vector(void);
