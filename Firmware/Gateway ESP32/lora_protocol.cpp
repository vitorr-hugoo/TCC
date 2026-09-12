/**
 * @file lora_protocol.cpp
 * @brief Implementa a validação e o estado de recepção LoRa do gateway.
 */

#include "lora_protocol.h"
#include <string.h>

uint8_t crc8_calc(const uint8_t *data, uint16_t len)
{
    uint8_t crc = 0;
    for (uint16_t i = 0; i < len; ++i)
    {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; ++bit)
        {
            crc = (uint8_t)((crc << 1) ^ ((crc & 0x80u) ? 0x07u : 0u));
        }
    }
    return crc;
}
bool payload_validate_crc(const LoRaPayload_t *p)
{
    return p && crc8_calc((const uint8_t *)p, sizeof(*p) - 1) == p->crc8;
}
void GatewayStart(GatewayState &state, uint32_t now)
{
    state = {};
    state.last_valid_ms = now;
}
PacketStatus GatewayAccept(GatewayState &state, const uint8_t *raw, size_t len, uint32_t now,
                           LoRaPayload_t &payload, bool &gap, bool &recovered)
{
    gap = recovered = false;
    if (!raw || len != EXPECTED_PAYLOAD_LEN)
    {
        return PacketStatus::Size;
    }
    LoRaPayload_t candidate;
    memcpy(&candidate, raw, sizeof(candidate));
    if (!payload_validate_crc(&candidate))
    {
        return PacketStatus::Crc;
    }
    if (candidate.node_id != 1)
    {
        return PacketStatus::Node;
    }
    gap = state.has_packet && candidate.seq != (uint16_t)(state.last_seq + 1u);
    if (gap)
    {
        ++state.sequence_gaps;
    }
    recovered = state.silent;
    state.silent = false;
    state.has_packet = true;
    state.last_valid_ms = now;
    state.last_seq = candidate.seq;
    ++state.valid_packets;
    payload = candidate;
    return PacketStatus::Valid;
}
bool GatewayCheckSilence(GatewayState &state, uint32_t now, uint32_t timeout_ms)
{
    if (!state.silent && (uint32_t)(now - state.last_valid_ms) >= timeout_ms)
    {
        state.silent = true;
        return true;
    }
    return false;
}
