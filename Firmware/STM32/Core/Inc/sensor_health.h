/**
 * @file sensor_health.h
 * @brief Declara o estado e as transições de saúde dos sensores da estação.
 */

#ifndef SENSOR_HEALTH_H
#define SENSOR_HEALTH_H
#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    SENSOR_DHT,
    SENSOR_BMP,
    SENSOR_BH,
    SENSOR_VANE,
    SENSOR_ANEM,
    SENSOR_RAIN,
    SENSOR_COUNT
} SensorId;
typedef enum
{
    SENSOR_OK,
    SENSOR_NOT_INITIALIZED,
    SENSOR_IO,
    SENSOR_TIMEOUT,
    SENSOR_CHECKSUM,
    SENSOR_RANGE,
    SENSOR_CONFIG,
    SENSOR_SATURATED,
    SENSOR_TIMER,
    SENSOR_CALIBRATION
} SensorReason;
typedef enum
{
    HEALTH_NONE,
    HEALTH_FIRST,
    HEALTH_FAILURE,
    HEALTH_REASON_CHANGED,
    HEALTH_PERSISTENT,
    HEALTH_RECOVERED
} HealthEvent;
#define SENSOR_FAILURE_THRESHOLD 3u
typedef struct
{
    bool observed, valid, has_valid;
    SensorReason reason;
    uint8_t consecutive_failures;
    uint32_t last_valid_ms;
} SensorHealth;
/* Zero initialization is sufficient. One update per completed acquisition. */
HealthEvent SensorHealth_Update(SensorHealth *health, SensorReason reason, uint32_t now);
const char *Sensor_Name(SensorId id);
const char *Sensor_Reason(SensorReason reason);
const char *Sensor_Event(HealthEvent event);
#endif
