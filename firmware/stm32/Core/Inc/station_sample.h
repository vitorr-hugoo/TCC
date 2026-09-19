/**
 * @file station_sample.h
 * @brief Declara validação e codificação da amostra da estação.
 */

#ifndef STATION_SAMPLE_H
#define STATION_SAMPLE_H
#include "sensor_health.h"
#include "lora_payload.h"
typedef struct
{
    uint32_t timestamp_ms;
    SensorReason reason[SENSOR_COUNT];
    float temperature, humidity, pressure, bmp_temperature, altitude;
    float illuminance, irradiance, wind_speed, rain_mm;
    uint16_t wind_dir_deg;
} StationSample;
/* Further validates candidates; never turns a driver error into success. */
void StationSample_Validate(StationSample *sample);
/* Legacy 18-byte encoding. Returns a bit mask of valid local values that
 * cannot fit the wire format; these use the existing invalid sentinel. */
uint8_t StationSample_Payload(const StationSample *sample, uint16_t seq, LoRaPayload_t *packet);
#endif
