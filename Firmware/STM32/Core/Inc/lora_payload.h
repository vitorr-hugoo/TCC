#ifndef LORA_PAYLOAD_H
#define LORA_PAYLOAD_H
#include <stdint.h>
#include <stddef.h>

/* Formato little-endian preservado no STM32 e ESP32. */
#pragma pack(push, 1)
typedef struct {
    uint8_t  node_id;
    uint16_t seq;
    int16_t  temperature_x10;
    uint16_t humidity_x10;
    uint16_t pressure_hpa_x1;
    uint16_t illuminance_lux_x1;
    int16_t  wind_speed_x100;
    uint16_t wind_dir_deg;
    uint16_t rain_mm_x100;
    uint8_t  crc8;
} LoRaPayload_t;
#pragma pack(pop)
_Static_assert(sizeof(LoRaPayload_t) == 18u, "Payload LoRa deve ter 18 bytes");
_Static_assert(offsetof(LoRaPayload_t, crc8) == 17u, "CRC deve ser o ultimo byte");

static inline uint8_t App_CRC8(const uint8_t *data, uint16_t len)
{
    uint8_t crc = 0x00u;

    for (uint16_t i = 0; i < len; i++)
    {
        crc ^= data[i];
        for (uint8_t b = 0; b < 8u; b++)
        {
            if (crc & 0x80u)
                crc = (uint8_t)((crc << 1) ^ 0x07u);
            else
                crc <<= 1;
        }
    }

    return crc;
}

#endif
