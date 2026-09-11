#include "station_sample.h"
#include <math.h>
#include <string.h>
static bool between(float x, float lo, float hi) { return isfinite(x) && x >= lo && x <= hi; }
void StationSample_Validate(StationSample *s)
{
    bool valid[SENSOR_COUNT] = {
        between(s->temperature, -40, 80) && between(s->humidity, 0, 100),
        between(s->pressure, 300, 1100) && between(s->bmp_temperature, -40, 85) && isfinite(s->altitude),
        between(s->illuminance, 0, 65535) && isfinite(s->irradiance) && s->irradiance >= 0,
        s->wind_dir_deg < 360,
        isfinite(s->wind_speed) && s->wind_speed >= 0,
        isfinite(s->rain_mm) && s->rain_mm >= 0
    };
    for (unsigned i = 0; i < SENSOR_COUNT; ++i)
        if (s->reason[i] == SENSOR_OK && !valid[i]) s->reason[i] = SENSOR_RANGE;
}
uint8_t StationSample_Payload(const StationSample *sample, uint16_t seq, LoRaPayload_t *p)
{
    StationSample checked = *sample;
    StationSample_Validate(&checked);
    const StationSample *s = &checked;
    uint8_t overflow = 0;
    memset(p, 0, sizeof(*p));
    p->node_id = 1;
    p->seq = seq;
    p->temperature_x10 = s->reason[SENSOR_DHT] == SENSOR_OK ? (int16_t)roundf(s->temperature * 10.0f) : -9990;
    p->humidity_x10 = s->reason[SENSOR_DHT] == SENSOR_OK ? (uint16_t)roundf(s->humidity * 10.0f) : 0;
    p->pressure_hpa_x1 = s->reason[SENSOR_BMP] == SENSOR_OK ? (uint16_t)s->pressure : 0;
    p->illuminance_lux_x1 = s->reason[SENSOR_BH] == SENSOR_OK ? (uint16_t)s->illuminance : 0;
    p->wind_dir_deg = s->reason[SENSOR_VANE] == SENSOR_OK ? s->wind_dir_deg : 999;
    p->wind_speed_x100 = -9990;
    if (s->reason[SENSOR_ANEM] == SENSOR_OK) {
        float scaled = roundf(s->wind_speed * 100.0f);
        if (between(scaled, 0, INT16_MAX)) p->wind_speed_x100 = (int16_t)scaled;
        else overflow |= 1u << SENSOR_ANEM;
    }
    if (s->reason[SENSOR_RAIN] == SENSOR_OK) {
        float scaled = roundf(s->rain_mm * 100.0f);
        if (between(scaled, 0, UINT16_MAX)) p->rain_mm_x100 = (uint16_t)scaled;
        else overflow |= 1u << SENSOR_RAIN;
    }
    p->crc8 = App_CRC8((const uint8_t *)p, sizeof(*p) - 1u);
    return overflow;
}
