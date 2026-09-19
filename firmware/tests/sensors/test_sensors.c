#include "fake_hal.h"
#include "dht22.h"
#include "bmp280.h"
#include "bh1750.h"
#include "wind_vane.h"
#include "anemometer.h"
#include "pluviometer.h"
#include "station_sample.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#undef assert
#define assert(x)                                                                                  \
    do                                                                                             \
    {                                                                                              \
        if (!(x))                                                                                  \
        {                                                                                          \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x);                                \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)

static TIM_TypeDef tim_regs = {1, 0, 0, 0};
static TIM_HandleTypeDef timer = {&tim_regs};
static GPIO_TypeDef gpio;
static I2C_HandleTypeDef i2c;
static ADC_HandleTypeDef adc = {.Instance = ADC1};
static BMP280_Handle_t bmp;
static BH1750_Handle_t bh;
static DHT22_Handle_t dht;
static void dht_init(void)
{
    DHT22_Init(&dht, &gpio, 1, &timer);
    dht.temperature = 12;
    dht.humidity = 34;
}
static void checksum(void)
{
    dht_raw[4] = (uint8_t)(dht_raw[0] + dht_raw[1] + dht_raw[2] + dht_raw[3]);
}
static void bmp_init(void)
{
    fake_bmp_vector();
    BMP280_Config_t cfg = BMP280_DEFAULT_CONFIG;
    assert(BMP280_Init(&bmp, &i2c, BMP280_ADDR_SDO_LOW, &cfg) == BMP280_OK);
}
static void bh_init(void)
{
    assert(BH1750_Init(&bh, &i2c, BH1750_ADDR_LOW, BH1750_MODE_ONCE_H_RES) == BH1750_OK);
}
static void vane_init(void)
{
    wind_vane_config_t cfg = {0};
    cfg.hadc = &adc;
    cfg.num_samples = 8;
    cfg.lut = WIND_VANE_DEFAULT_LUT;
    cfg.lut_size = WIND_VANE_DEFAULT_LUT_SIZE;
    assert(Wind_Vane_Init(&cfg) == HAL_OK);
}
static void test_dht_valid(void)
{
    dht_init();
    assert(DHT22_Read(&dht) == DHT22_OK);
    assert(fabsf(dht.temperature - 23.4f) < .01f);
    assert(fabsf(dht.humidity - 55.5f) < .01f);
}
static void test_dht_negative(void)
{
    dht_init();
    dht_raw[2] = 0x80;
    dht_raw[3] = 100;
    checksum();
    assert(DHT22_Read(&dht) == DHT22_OK);
    assert(dht.temperature == -10);
}
static void test_dht_zero(void)
{
    dht_init();
    memset(dht_raw, 0, 5);
    assert(DHT22_Read(&dht) == DHT22_OK);
    assert(dht.temperature == 0 && dht.humidity == 0);
}
static void test_dht_checksum(void)
{
    dht_init();
    dht_raw[4] ^= 1;
    assert(DHT22_Read(&dht) == DHT22_ERR_CHECKSUM);
    assert(dht.temperature == 12 && dht.humidity == 34);
}
static void test_dht_range(void)
{
    dht_init();
    dht_raw[0] = 4;
    checksum();
    assert(DHT22_Read(&dht) == DHT22_ERR_RANGE);
    assert(dht.humidity == 34);
}
static void test_dht_temp_range(void)
{
    dht_init();
    dht_raw[2] = 3;
    dht_raw[3] = 33;
    checksum();
    assert(DHT22_Read(&dht) == DHT22_ERR_RANGE);
    assert(dht.temperature == 12);
}
static void test_dht_stuck_high(void)
{
    dht_init();
    dht_stuck = 1;
    assert(DHT22_Read(&dht) == DHT22_ERR_TIMEOUT);
    assert(fake_us < 1000);
}
static void test_dht_stuck_low(void)
{
    dht_init();
    dht_stuck = 2;
    assert(DHT22_Read(&dht) == DHT22_ERR_TIMEOUT);
}
static void test_dht_timer_off(void)
{
    dht_init();
    tim_regs.CR1 = 0;
    assert(DHT22_Read(&dht) == DHT22_ERR_TIMER);
}
static void test_dht_timer_stopped(void)
{
    dht_init();
    timer_stuck = 1;
    dht_stuck = 1;
    assert(DHT22_Read(&dht) == DHT22_ERR_TIMER);
    assert(dht_counter_reads < 100002);
}
static void test_dht_bad_pulse(void)
{
    dht_init();
    bad_pulse = 1;
    assert(DHT22_Read(&dht) == DHT22_ERR_RANGE);
}
static void test_dht_counter_wrap(void)
{
    dht_init();
    fake_us = 65500;
    assert(DHT22_Read(&dht) == DHT22_OK);
}
static void test_dht_recovery(void)
{
    dht_init();
    dht_stuck = 1;
    assert(DHT22_Read(&dht) != DHT22_OK);
    fake_ms += 5000;
    dht_stuck = 0;
    assert(DHT22_Read(&dht) == DHT22_OK);
}
static void test_bmp_vector(void)
{
    bmp_init();
    assert(BMP280_Read(&bmp) == BMP280_OK);
    assert(fabsf(bmp.temperature - 25.08f) < .02f);
    assert(fabsf(bmp.pressure - 1006.53f) < .03f);
}
static void test_bmp_identity(void)
{
    fake_bmp_vector();
    bmp_regs[0xd0] = 0x60;
    BMP280_Config_t c = BMP280_DEFAULT_CONFIG;
    assert(BMP280_Init(&bmp, &i2c, 0x76, &c) == BMP280_ERR_NOT_FOUND);
    assert(!bmp.initialized);
}
static void test_bmp_calibration(void)
{
    fake_bmp_vector();
    bmp_regs[0x8e] = bmp_regs[0x8f] = 0;
    BMP280_Config_t c = BMP280_DEFAULT_CONFIG;
    assert(BMP280_Init(&bmp, &i2c, 0x76, &c) == BMP280_ERR_CALIBRATION);
    assert(BMP280_Read(&bmp) == BMP280_ERR_CONFIG);
}
static void test_bmp_init_mismatch(void)
{
    fake_bmp_vector();
    corrupt_config = 1;
    BMP280_Config_t c = BMP280_DEFAULT_CONFIG;
    assert(BMP280_Init(&bmp, &i2c, 0x76, &c) == BMP280_ERR_CONFIG);
}
static void test_bmp_read_nack(void)
{
    bmp_init();
    assert(BMP280_Read(&bmp) == BMP280_OK);
    float old = bmp.pressure;
    i2c_result = HAL_ERROR;
    assert(BMP280_Read(&bmp) == BMP280_ERR_I2C);
    assert(bmp.pressure == old);
    i2c_result = HAL_OK;
    assert(BMP280_Read(&bmp) == BMP280_OK);
    assert(bmp_reset_count == 1);
}
static void test_bmp_lost_config(void)
{
    bmp_init();
    bmp_regs[0xf4] = 0;
    assert(BMP280_Read(&bmp) == BMP280_ERR_CONFIG);
    assert(!bmp.initialized);
    assert(BMP280_Read(&bmp) == BMP280_ERR_CONFIG);
}
static void test_bmp_final_config(void)
{
    bmp_init();
    fail_after_data = 1;
    assert(BMP280_Read(&bmp) == BMP280_ERR_CONFIG);
    assert(bmp.pressure == 0);
}
static void test_bmp_busy_ready(void)
{
    bmp_init();
    bmp_busy_reads = 3;
    uint32_t start = fake_ms;
    assert(BMP280_Read(&bmp) == BMP280_OK);
    assert(fake_ms - start == 3);
}
static void test_bmp_busy_timeout(void)
{
    bmp_init();
    bmp_busy_reads = 1000;
    uint32_t start = fake_ms;
    assert(BMP280_Read(&bmp) == BMP280_ERR_BUSY);
    assert(fake_ms - start == bmp.measurement_timeout_ms);
    assert(bmp.measurement_timeout_ms == 49);
}
static void test_bmp_disabled_raw(void)
{
    bmp_init();
    bmp_regs[0xf7] = 0x80;
    bmp_regs[0xf8] = bmp_regs[0xf9] = 0;
    assert(BMP280_Read(&bmp) == BMP280_ERR_RANGE);
    assert(bmp.pressure == 0);
}
static void test_bmp_zero_divisor(void)
{
    bmp_init();
    bmp.dig_P1 = 0;
    assert(BMP280_Read(&bmp) == BMP280_ERR_RANGE);
    assert(bmp.pressure == 0);
}
static void test_bmp_same_value(void)
{
    bmp_init();
    assert(BMP280_Read(&bmp) == BMP280_OK);
    float p = bmp.pressure;
    fake_ms += 5000;
    assert(BMP280_Read(&bmp) == BMP280_OK);
    assert(p == bmp.pressure);
    assert(bmp_reset_count == 1);
}
static void test_bmp_forced(void)
{
    fake_bmp_vector();
    BMP280_Config_t c = BMP280_DEFAULT_CONFIG;
    c.mode = BMP280_MODE_FORCED;
    assert(BMP280_Init(&bmp, &i2c, 0x76, &c) == BMP280_OK);
    bmp_regs[0xf4] &= 0xfc;
    assert(BMP280_Read(&bmp) == BMP280_OK);
}
static void test_bmp_bad_config(void)
{
    BMP280_Config_t c = BMP280_DEFAULT_CONFIG;
    c.os_pressure = BMP280_OS_SKIPPED;
    assert(BMP280_Init(&bmp, &i2c, 0x76, &c) == BMP280_ERR_CONFIG);
}
static void test_bh_fresh(void)
{
    bh_init();
    uint32_t start = fake_ms;
    unsigned cmds = bh_commands;
    assert(BH1750_Read(&bh) == BH1750_OK);
    assert(fake_ms - start == 180 && bh_commands - cmds == 2);
    assert(fabsf(bh.illuminance - 1000) < .01f);
    assert(BH1750_Read(&bh) == BH1750_OK);
    assert(bh_commands - cmds == 4);
}
static void test_bh_dark(void)
{
    bh_init();
    bh_raw = 0;
    assert(BH1750_Read(&bh) == BH1750_OK);
    assert(bh.illuminance == 0 && bh.irradiance == 0);
}
static void test_bh_saturated(void)
{
    bh_init();
    assert(BH1750_Read(&bh) == BH1750_OK);
    float old = bh.irradiance;
    bh_raw = 65535;
    assert(BH1750_Read(&bh) == BH1750_ERR_SATURATED);
    assert(bh.irradiance == old);
}
static void test_bh_nack(void)
{
    bh_init();
    i2c_result = HAL_TIMEOUT;
    assert(BH1750_Read(&bh) == BH1750_ERR_I2C);
    i2c_result = HAL_OK;
    assert(BH1750_Read(&bh) == BH1750_OK);
}
static void test_bh_receive_timeout(void)
{
    bh_init();
    assert(BH1750_Read(&bh) == BH1750_OK);
    float old = bh.illuminance;
    fail_receive = 1;
    assert(BH1750_Read(&bh) == BH1750_ERR_I2C);
    assert(bh.illuminance == old);
}
static void test_bmp_receive_timeout(void)
{
    bmp_init();
    fail_receive = 1;
    assert(BMP280_Read(&bmp) == BMP280_ERR_I2C);
    assert(bmp.pressure == 0);
}
static void test_bh_bad_factor(void)
{
    bh_init();
    BH1750_SetCalibration(&bh, NAN);
    assert(BH1750_Read(&bh) == BH1750_ERR_RANGE);
    assert(bh.illuminance == 0);
}
static void test_bh_init_fail(void)
{
    i2c_result = HAL_ERROR;
    assert(BH1750_Init(&bh, &i2c, 0x23, BH1750_MODE_ONCE_H_RES) == BH1750_ERR_I2C);
    i2c_result = HAL_OK;
    assert(BH1750_Read(&bh) == BH1750_ERR_CONFIG);
}
static void test_bh_not_ready(void)
{
    assert(BH1750_Init(&bh, &i2c, 0x23, BH1750_MODE_CONT_H_RES) == BH1750_OK);
    assert(BH1750_Read(&bh) == BH1750_ERR_NOT_RDY);
    fake_ms += 180;
    assert(BH1750_Read(&bh) == BH1750_OK);
}
static void test_vane_majority(void)
{
    vane_init();
    for (int i = 0; i < 8; i++)
    {
        adc_values[i] = i < 4 ? 1000 : 100;
    }
    uint16_t deg = 999;
    assert(Wind_Vane_Read(&deg) == HAL_ERROR);
    assert(deg == 999 && !fake_adc_clock);
    adc_index = 0;
    adc_values[4] = 1000;
    assert(Wind_Vane_Read(&deg) == HAL_OK);
    assert(deg == 90);
}
static void test_vane_boundaries(void)
{
    vane_init();
    for (unsigned j = 0; j < WIND_VANE_DEFAULT_LUT_SIZE; j++)
    {
        for (unsigned edge = 0; edge < 2; edge++)
        {
            adc_index = 0;
            for (int i = 0; i < 8; i++)
            {
                adc_values[i] =
                    edge ? WIND_VANE_DEFAULT_LUT[j].adc_max : WIND_VANE_DEFAULT_LUT[j].adc_min;
            }
            uint16_t deg;
            assert(Wind_Vane_Read(&deg) == HAL_OK);
            assert(deg == WIND_VANE_DEFAULT_LUT[j].degrees);
        }
    }
}
static void test_vane_start_fail(void)
{
    vane_init();
    adc_start = HAL_ERROR;
    uint16_t deg = 999;
    assert(Wind_Vane_Read(&deg) == HAL_ERROR);
    assert(deg == 999 && !fake_adc_clock);
}
static void test_vane_timeout(void)
{
    vane_init();
    adc_poll = HAL_TIMEOUT;
    uint16_t deg = 999;
    assert(Wind_Vane_Read(&deg) == HAL_TIMEOUT);
    assert(deg == 999 && !fake_adc_clock);
}
static void test_vane_stop_fail(void)
{
    vane_init();
    adc_stop = HAL_ERROR;
    uint16_t deg = 999;
    assert(Wind_Vane_Read(&deg) == HAL_ERROR);
    assert(deg == 999 && !fake_adc_clock);
}
static void test_vane_init_fail(void)
{
    vane_init();
    assert(Wind_Vane_Init(NULL) == HAL_ERROR);
    uint16_t deg;
    assert(Wind_Vane_Read(&deg) == HAL_ERROR);
}
static void test_anem_calm(void)
{
    anemometer_init();
    assert(anemometer_start_sampling(&timer) == HAL_OK);
    fake_ms = 1000;
    anemometer_stop_sampling(&timer);
    assert(anemometer_service(&timer, fake_ms) == HAL_OK);
    Anemometer_Data_t data;
    anemometer_calculate(1000, &data);
    assert(data.valid && data.speed_ms == 0);
}
static void test_anem_start_fail(void)
{
    anemometer_init();
    timer_start = HAL_ERROR;
    assert(anemometer_start_sampling(&timer) == HAL_ERROR);
    assert(anemometer_service(&timer, 0) == HAL_ERROR);
    assert(!(tim_regs.CR1 & 1));
}
static void test_anem_start_busy(void)
{
    anemometer_init();
    timer_start = HAL_BUSY;
    assert(anemometer_start_sampling(&timer) != HAL_OK);
    assert(anemometer_service(&timer, 0) == HAL_ERROR);
}
static void test_anem_stop_fail(void)
{
    anemometer_init();
    anemometer_start_sampling(&timer);
    fake_ms = 1000;
    timer_stop = HAL_ERROR;
    anemometer_stop_sampling(&timer);
    assert(anemometer_service(&timer, fake_ms) == HAL_ERROR);
    assert(!anemometer_is_ready() && !(tim_regs.CR1 & 1));
}
static void test_anem_timeout_late_irq(void)
{
    anemometer_init();
    anemometer_start_sampling(&timer);
    assert(anemometer_service(&timer, 1999) == HAL_BUSY);
    assert(anemometer_service(&timer, 2000) == HAL_TIMEOUT);
    fake_ms = 2100;
    anemometer_stop_sampling(&timer);
    assert(!anemometer_is_ready());
    anemometer_start_sampling(&timer);
    anemometer_stop_sampling(&timer);
    assert(!anemometer_is_ready());
    fake_ms += 1000;
    anemometer_stop_sampling(&timer);
    assert(anemometer_is_ready());
}
static void test_anem_wrap(void)
{
    anemometer_init();
    fake_ms = UINT32_MAX - 500;
    anemometer_start_sampling(&timer);
    fake_ms += 1000;
    anemometer_stop_sampling(&timer);
    assert(anemometer_is_ready());
}
static void test_anem_pulses_irq(void)
{
    anemometer_init();
    fake_irq = 1;
    anemometer_start_sampling(&timer);
    assert(fake_irq == 1);
    fake_ms = 100;
    anemometer_pulse_callback();
    fake_ms = 105;
    anemometer_pulse_callback();
    fake_ms = 125;
    anemometer_pulse_callback();
    uint32_t a, r, t;
    anemometer_get_diagnostics(&a, &r, &t);
    assert(a == 2 && r == 1 && t == 125 && fake_irq == 1);
    fake_ms = 1000;
    anemometer_stop_sampling(&timer);
    assert(fake_irq == 1);
    Anemometer_Data_t d;
    anemometer_calculate(1000, &d);
    assert(d.valid && d.pulse_count == 2);
    anemometer_calculate(1000, &d);
    assert(!d.valid);
}
static void test_rain_dry(void)
{
    pluviometer_init();
    Pluviometer_Data_t d;
    pluviometer_read(&d);
    assert(d.valid && d.rain_mm == 0 && d.delta_tips == 0);
}
static void test_rain_counts(void)
{
    pluviometer_init();
    fake_ms = 200;
    pluviometer_tip_callback();
    fake_ms = 210;
    pluviometer_tip_callback();
    fake_ms = 400;
    pluviometer_tip_callback();
    Pluviometer_Data_t d;
    fake_irq = 1;
    pluviometer_read(&d);
    assert(fake_irq == 1 && d.tip_count == 2 && d.delta_tips == 2);
    pluviometer_read(&d);
    assert(d.tip_count == 2 && d.delta_tips == 0);
    uint32_t r, t;
    pluviometer_get_diagnostics(&r, &t);
    assert(r == 1 && t == 400 && fake_irq == 1);
}
static void test_rain_wrap(void)
{
    pluviometer_init();
    fake_ms = UINT32_MAX - 100;
    pluviometer_tip_callback();
    fake_ms += 200;
    pluviometer_tip_callback();
    assert(pluviometer_get_tip_count() == 2);
}
static void test_health_transitions(void)
{
    SensorHealth h = {0};
    assert(SensorHealth_Update(&h, SENSOR_OK, 0) == HEALTH_FIRST);
    assert(h.has_valid && h.valid && h.last_valid_ms == 0);
    assert(SensorHealth_Update(&h, SENSOR_TIMEOUT, 5) == HEALTH_FAILURE);
    assert(!h.valid && h.last_valid_ms == 0);
    assert(SensorHealth_Update(&h, SENSOR_TIMEOUT, 10) == HEALTH_NONE);
    assert(SensorHealth_Update(&h, SENSOR_TIMEOUT, 15) == HEALTH_PERSISTENT);
    assert(SensorHealth_Update(&h, SENSOR_TIMEOUT, 20) == HEALTH_NONE);
    assert(SensorHealth_Update(&h, SENSOR_OK, 25) == HEALTH_RECOVERED);
    assert(h.valid && h.last_valid_ms == 25 && h.consecutive_failures == 0);
}
static void test_health_initial_failure(void)
{
    SensorHealth h = {0};
    assert(!h.observed && !h.has_valid);
    assert(SensorHealth_Update(&h, SENSOR_CONFIG, 5) == HEALTH_FIRST);
    assert(!h.has_valid && !h.valid);
    assert(SensorHealth_Update(&h, SENSOR_IO, 10) == HEALTH_REASON_CHANGED);
}
static void test_health_saturation(void)
{
    SensorHealth h = {0};
    for (int i = 0; i < 1000; i++)
    {
        SensorHealth_Update(&h, SENSOR_IO, i);
    }
    assert(h.consecutive_failures == 255);
    assert(SensorHealth_Update(&h, SENSOR_IO, 1001) == HEALTH_NONE);
}
static void test_health_wrap(void)
{
    SensorHealth h = {0};
    SensorHealth_Update(&h, SENSOR_OK, UINT32_MAX - 10);
    SensorHealth_Update(&h, SENSOR_IO, 10);
    assert((uint32_t)(10 - h.last_valid_ms) == 21);
}
static StationSample good_sample(void)
{
    StationSample s = {0};
    s.temperature = 23;
    s.humidity = 55;
    s.pressure = 1000;
    s.bmp_temperature = 25;
    s.altitude = 30;
    s.illuminance = 1000;
    s.irradiance = 8;
    s.wind_speed = 1;
    s.wind_dir_deg = 90;
    s.rain_mm = 1;
    return s;
}
static void test_sample_crc(void)
{
    StationSample s = good_sample();
    LoRaPayload_t p;
    assert(StationSample_Payload(&s, 42, &p) == 0);
    assert(sizeof(p) == 18 && p.seq == 42 && p.temperature_x10 == 230 && p.rain_mm_x100 == 100);
    assert(p.crc8 == App_CRC8((uint8_t *)&p, 17));
}
static void test_sample_stale(void)
{
    StationSample s = good_sample();
    s.reason[SENSOR_DHT] = SENSOR_CHECKSUM;
    LoRaPayload_t p;
    StationSample_Payload(&s, 0, &p);
    assert(p.temperature_x10 == -9990 && p.humidity_x10 == 0);
    assert(p.pressure_hpa_x1 == 1000 && p.wind_speed_x100 == 100);
}
static void test_sample_nonfinite(void)
{
    StationSample s = good_sample();
    s.temperature = NAN;
    s.pressure = INFINITY;
    s.irradiance = NAN;
    s.wind_speed = INFINITY;
    s.rain_mm = NAN;
    s.wind_dir_deg = 999;
    StationSample_Validate(&s);
    for (int i = 0; i < SENSOR_COUNT; i++)
    {
        assert(s.reason[i] == SENSOR_RANGE);
    }
    LoRaPayload_t p;
    StationSample_Payload(&s, 0, &p);
    assert(p.rain_mm_x100 == 0 && p.wind_speed_x100 == -9990);
}
static void test_sample_wire_overflow(void)
{
    StationSample s = good_sample();
    s.rain_mm = 700;
    s.wind_speed = 400;
    StationSample_Validate(&s);
    assert(s.reason[SENSOR_RAIN] == SENSOR_OK);
    LoRaPayload_t p;
    assert(StationSample_Payload(&s, 0, &p) == ((1u << SENSOR_RAIN) | (1u << SENSOR_ANEM)));
    assert(p.rain_mm_x100 == 0 && p.wind_speed_x100 == -9990);
    assert(s.rain_mm == 700 && p.pressure_hpa_x1 == 1000);
}
static void test_sample_wire_boundary(void)
{
    StationSample s = good_sample();
    s.rain_mm = 655.35f;
    s.wind_speed = 327.67f;
    LoRaPayload_t p;
    assert(StationSample_Payload(&s, 0, &p) == 0);
    assert(p.rain_mm_x100 == 65535 && p.wind_speed_x100 == 32767);
}

/* The runner generates only the test-name dispatch, never driver logic. */
#include "test_dispatch.h"
