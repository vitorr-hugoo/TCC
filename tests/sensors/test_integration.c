#include "fake_hal.h"
#include "fake_storage.h"
#include "storage.h"
#include "dht22.h"
#include "bmp280.h"
#include "bh1750.h"
#include "anemometer.h"
#include "pluviometer.h"
#include "wind_vane.h"
#include "station_sample.h"
#include "lora_sx1276.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define assert(x) do { if (!(x)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x); exit(1); } } while(0)
static TIM_TypeDef tim6_regs, tim10_regs = {1,0,0,0};
TIM_HandleTypeDef htim6 = {&tim6_regs}, htim10 = {&tim10_regs};
ADC_HandleTypeDef hadc1 = {ADC1};
I2C_HandleTypeDef hi2c1;
static GPIO_TypeDef gpio;
static unsigned transmissions;
static LoRaPayload_t last_packet;
bool SX1276_Transmit(const uint8_t *data, uint8_t len) {
    assert(len==18);memcpy(&last_packet,data,len);++transmissions;return true;
}
const char *SX1276_LastError(void) {return "OK";}
/* Extracted verbatim from main.c on every test run: actual application
 * globals, USER CODE 0, and one iteration of the acquisition loop. */
#include "app_under_test.h"
static void setup(void) {
    fake_reset();fake_bmp_vector();
    DHT22_Init(&dht22,&gpio,1,&htim10);
    BMP280_Config_t cfg=BMP280_DEFAULT_CONFIG;
    last_bmp_status=BMP280_Init(&bmp280,&hi2c1,0x76,&cfg);
    last_bh_status=BH1750_Init(&bh1750,&hi2c1,0x23,BH1750_MODE_ONCE_H_RES);
    assert(last_bmp_status==BMP280_OK && last_bh_status==BH1750_OK);
    anemometer_init();pluviometer_init();adc_initialized=true;App_WindVane_Init();
    for(int i=0;i<16;i++)adc_values[i]=3100;
    (void)Storage_Init();fake_ms=5000;
}
static void finish_window(void) {
    fake_ms=acquisition_start_ms+1000-fake_us/1000;
    anemometer_stop_sampling(&htim6);iteration();
}
static void test_integration_normal(void) {
    setup();iteration();assert(acquisition_pending && transmissions==0);finish_window();
    assert(!acquisition_pending && transmissions==1);
    assert(strstr(data_log,"5000;23.4;55.5;1006.") && strstr(data_log,";0.00;0.0;0.00"));
    for(int i=0;i<SENSOR_COUNT;i++)assert(sensor_health[i].valid);
    assert(last_packet.wind_speed_x100==0 && last_packet.wind_dir_deg==0);
}
static void test_integration_timer_timeout(void) {
    setup();iteration();fake_ms=7000;iteration();
    assert(!acquisition_pending && transmissions==1);
    assert(!sensor_health[SENSOR_ANEM].valid && sensor_health[SENSOR_ANEM].reason==SENSOR_TIMEOUT);
    assert(sensor_health[SENSOR_DHT].valid && sensor_health[SENSOR_BMP].valid);
    assert(last_packet.wind_speed_x100==-9990 && last_packet.temperature_x10==234);
    assert(strstr(data_log,";-999.00;") && strstr(health_log,"ANEM;PRIMEIRA_LEITURA;0;TIMEOUT"));
}
static void test_integration_timer_start_error(void) {
    setup();timer_start=HAL_ERROR;iteration();assert(!acquisition_pending && transmissions==1);
    assert(sensor_health[SENSOR_ANEM].reason==SENSOR_TIMER && sensor_health[SENSOR_BH].valid);
}
static void test_integration_dht_stale(void) {
    setup();dht22.temperature=77;dht22.humidity=66;dht_stuck=1;iteration();finish_window();
    assert(last_packet.temperature_x10==-9990 && last_packet.humidity_x10==0);
    assert(dht22.temperature==77 && strstr(data_log,";-999.0;-999.0;"));
    assert(sensor_health[SENSOR_BMP].valid);
}
static void test_integration_adc_error(void) {
    setup();adc_poll=HAL_TIMEOUT;iteration();finish_window();
    assert(last_packet.wind_dir_deg==999 && last_packet.pressure_hpa_x1==1006);
    assert(sensor_health[SENSOR_VANE].reason==SENSOR_TIMEOUT);
}
static void test_integration_actual_lut(void) {
    setup();
    for(unsigned pos=0;pos<16;pos++) for(unsigned edge=0;edge<2;edge++) {
        adc_index=0;
        for(unsigned j=0;j<8;j++) adc_values[j]=edge ? s_vane_lut[pos].adc_max : s_vane_lut[pos].adc_min;
        uint16_t deg=999;
        assert(Wind_Vane_Read(&deg)==HAL_OK && deg==s_vane_lut[pos].degrees);
    }
}
static void test_integration_shared_i2c(void) {
    setup();i2c_result=HAL_ERROR;iteration();finish_window();
    assert(!sensor_health[SENSOR_BMP].valid && !sensor_health[SENSOR_BH].valid);
    assert(sensor_health[SENSOR_DHT].valid && transmissions==1);
}
static void test_integration_init_failed(void) {
    setup();bmp280.initialized=0;last_bmp_status=BMP280_ERR_NOT_FOUND;
    bh1750.initialized=0;last_bh_status=BH1750_ERR_I2C;
    assert(Wind_Vane_Init(NULL)==HAL_ERROR);
    iteration();finish_window();
    assert(!sensor_health[SENSOR_BMP].valid && !sensor_health[SENSOR_VANE].valid);
    assert(sensor_health[SENSOR_DHT].valid && transmissions==1);
}
static void test_integration_sd_absent(void) {
    mount_result=FR_NOT_READY;setup();iteration();finish_window();
    assert(transmissions==1 && sensor_health[SENSOR_DHT].valid && !Storage_IsReady());
}
static void test_integration_health_write_failure(void) {
    setup();short_at_write=write_count+1;iteration();finish_window();
    assert(transmissions==1 && !Storage_IsReady() && Storage_GetLastError()==FR_DISK_ERR);
}
static void test_integration_rain_overflow(void) {
    setup();for(int i=0;i<2500;i++){fake_ms+=200;pluviometer_tip_callback();}
    iteration();finish_window();
    assert(sensor_health[SENSOR_RAIN].valid && last_dados_chuva.rain_mm>655.35f);
    assert(last_packet.rain_mm_x100==0 && last_packet.pressure_hpa_x1==1006);
    assert(strstr(data_log,";698.50") && pluviometer_get_tip_count()==2500);
}
static void test_integration_log_transitions(void) {
    setup();dht_stuck=1;
    for(int cycle=0;cycle<4;cycle++){
        adc_index=0;fake_ms=last_dht_ms+5000;iteration();finish_window();
    }
    assert(sensor_health[SENSOR_DHT].consecutive_failures==4);
    char *first=strstr(health_log,"DHT22;PRIMEIRA_LEITURA;0;");assert(first);
    char *persistent=strstr(first,"DHT22;PERSISTENTE;0;");assert(persistent);
    assert(!strstr(persistent+5,"DHT22;"));
    dht_stuck=0;adc_index=0;fake_ms=last_dht_ms+5000;iteration();finish_window();
    assert(strstr(health_log,"DHT22;RECUPERADO;1;OK;0;1;"));
    assert(sensor_health[SENSOR_DHT].valid && transmissions==5);
}
static void test_storage_health_close_failure(void) {
    setup();SensorHealth h={0};SensorHealth_Update(&h,SENSOR_OK,0);close_result=FR_DISK_ERR;
    assert(Storage_AppendSensorHealth(0,SENSOR_DHT,&h,HEALTH_FIRST)==FR_DISK_ERR);
    assert(!Storage_IsReady());
}
static void test_storage_health_open_failure(void) {
    setup();SensorHealth h={0};fail_health_open=1;
    assert(Storage_AppendSensorHealth(0,SENSOR_DHT,&h,HEALTH_FIRST)==FR_DISK_ERR);
    assert(!Storage_IsReady());
}
#include "integration_dispatch.h"
