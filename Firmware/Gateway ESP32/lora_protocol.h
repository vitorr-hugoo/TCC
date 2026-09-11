#ifndef GATEWAY_LORA_PROTOCOL_H
#define GATEWAY_LORA_PROTOCOL_H
#include <stdint.h>
#include <stddef.h>

/* Contrato little-endian preservado; espelha lora_payload.h do STM32. */
constexpr size_t EXPECTED_PAYLOAD_LEN = 18;
constexpr uint32_t LORA_SILENCE_TIMEOUT_MS = 20000;
#pragma pack(push, 1)
struct LoRaPayload_t {
    uint8_t node_id;
    uint16_t seq;
    int16_t temperature_x10;
    uint16_t humidity_x10, pressure_hpa_x1, illuminance_lux_x1;
    int16_t wind_speed_x100;
    uint16_t wind_dir_deg, rain_mm_x100;
    uint8_t crc8;
};
#pragma pack(pop)
static_assert(sizeof(LoRaPayload_t) == EXPECTED_PAYLOAD_LEN, "Payload deve ter 18 bytes");
static_assert(offsetof(LoRaPayload_t, crc8) == 17, "CRC deve ser o ultimo byte");

uint8_t crc8_calc(const uint8_t *data, uint16_t len);
bool payload_validate_crc(const LoRaPayload_t *p);
enum class PacketStatus { Valid, Size, Crc, Node };
struct GatewayState {
    uint32_t last_valid_ms, valid_packets, sequence_gaps;
    uint16_t last_seq;
    bool has_packet, silent;
};
void GatewayStart(GatewayState &state, uint32_t now);
PacketStatus GatewayAccept(GatewayState &state, const uint8_t *raw, size_t len,
                           uint32_t now, LoRaPayload_t &payload, bool &gap, bool &recovered);
/* true somente na transicao para silencio; calculo seguro ao millis voltar a zero. */
bool GatewayCheckSilence(GatewayState &state, uint32_t now,
                         uint32_t timeout_ms = LORA_SILENCE_TIMEOUT_MS);
#endif
