#include "lora_protocol.h"
#include "lora_receive.h"
#include "fixtures.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#define REQUIRE(x)                                                                                 \
    do                                                                                             \
    {                                                                                              \
        if (!(x))                                                                                  \
        {                                                                                          \
            fprintf(stderr, "FAIL linha %d: %s\n", __LINE__, #x);                                  \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

struct FakeRadio
{
    std::vector<uint8_t> bytes;
    int announced, index = 0, read_error_at = -1;
    bool stuck = false;
    explicit FakeRadio(int size) : bytes(size > 0 ? size : 0, 0), announced(size)
    {
    }
    int parsePacket()
    {
        return announced;
    }
    int available()
    {
        return stuck ? 1 : (int)bytes.size() - index;
    }
    int read()
    {
        if (index == read_error_at)
        {
            return -1;
        }
        if (index >= (int)bytes.size())
        {
            ++index;
            return 0xff;
        }
        return bytes[index++];
    }
};
static void SetSequence(uint8_t *p, uint16_t seq)
{
    p[1] = (uint8_t)seq;
    p[2] = (uint8_t)(seq >> 8);
    p[17] = crc8_calc(p, 17);
}
static bool SameState(const GatewayState &a, const GatewayState &b)
{
    return a.last_valid_ms == b.last_valid_ms && a.last_seq == b.last_seq &&
           a.valid_packets == b.valid_packets && a.sequence_gaps == b.sequence_gaps &&
           a.has_packet == b.has_packet && a.silent == b.silent;
}
int main(int argc, char **argv)
{
    if (argc != 2)
    {
        return 2;
    }
    int test = atoi(argv[1]);
    GatewayState state;
    GatewayStart(state, 0);
    LoRaPayload_t output = {};
    bool gap = false, recovered = false;
    uint8_t packet[18];
    memcpy(packet, golden_packet, 18);
    auto accept = [&](uint32_t now)
    { return GatewayAccept(state, packet, 18, now, output, gap, recovered); };
    switch (test)
    {
        case 1:
            REQUIRE(crc8_calc((const uint8_t *)"123456789", 9) == 0xf4);
            REQUIRE(crc8_calc(golden_packet, 17) == 0x3f);
            break;
        case 2: /* Independente do CRC STM32: exige o mesmo vetor fixo. */
            REQUIRE(accept(100) == PacketStatus::Valid);
            REQUIRE(output.crc8 == 0x3f && payload_validate_crc(&output));
            break;
        case 3:
            REQUIRE(accept(100) == PacketStatus::Valid);
            REQUIRE(output.node_id == 1 && output.seq == 42 && output.temperature_x10 == 250);
            REQUIRE(output.humidity_x10 == 600 && output.pressure_hpa_x1 == 1013 &&
                    output.illuminance_lux_x1 == 1000);
            REQUIRE(output.wind_speed_x100 == 250 && output.wind_dir_deg == 180 &&
                    output.rain_mm_x100 == 123);
            REQUIRE(memcmp(&output, golden_packet, 18) == 0);
            REQUIRE(state.valid_packets == 1 && state.last_valid_ms == 100);
            break;
        case 4:
            for (int bit = 0; bit < 144; ++bit)
            {
                memcpy(packet, golden_packet, 18);
                packet[bit / 8] ^= 1u << (bit % 8);
                REQUIRE(accept(500) == PacketStatus::Crc);
                REQUIRE(!state.has_packet && state.valid_packets == 0 && state.last_valid_ms == 0);
            }
            break;
        case 5:
        {
            uint8_t oversized[255] = {};
            for (size_t len : {0u, 1u, 17u, 19u, 255u})
            {
                REQUIRE(GatewayAccept(state, oversized, len, 0, output, gap, recovered) ==
                        PacketStatus::Size);
            }
            REQUIRE(GatewayAccept(state, nullptr, 18, 0, output, gap, recovered) ==
                    PacketStatus::Size);
            break;
        }
        case 6:
        {
            FakeRadio radio(19);
            memcpy(radio.bytes.data(), packet, 18);
            uint8_t buf[20];
            memset(buf, 0xa5, sizeof buf);
            REQUIRE(LoRaReadPacket(radio, buf + 1, 18) == 0);
            REQUIRE(radio.index == 19);
            for (uint8_t b : buf)
            {
                REQUIRE(b == 0xa5); // prefixo nao copiado nem overflow
            }
            break;
        }
        case 7:
        {
            FakeRadio radio(18);
            radio.bytes.resize(17);
            REQUIRE(LoRaReadPacket(radio, packet, 18) == 0);
            FakeRadio read_error(18);
            read_error.read_error_at = 5;
            REQUIRE(LoRaReadPacket(read_error, packet, 18) == 0);
            break;
        }
        case 8:
            packet[0] = 2;
            packet[17] = crc8_calc(packet, 17);
            REQUIRE(accept(100) == PacketStatus::Node);
            REQUIRE(state.valid_packets == 0);
            break;
        case 9:
        {
            REQUIRE(accept(100) == PacketStatus::Valid);
            GatewayState before = state;
            LoRaPayload_t previous = output;
            packet[2] ^= 0x80;
            REQUIRE(accept(1000) == PacketStatus::Crc);
            REQUIRE(SameState(state, before) && memcmp(&output, &previous, 18) == 0);
            REQUIRE(!gap && !recovered);
            break;
        }
        case 10:
            REQUIRE(accept(0) == PacketStatus::Valid && !gap);
            SetSequence(packet, 43);
            REQUIRE(accept(5000) == PacketStatus::Valid && !gap);
            SetSequence(packet, 45);
            REQUIRE(accept(10000) == PacketStatus::Valid && gap);
            REQUIRE(state.sequence_gaps == 1);
            break;
        case 11:
            SetSequence(packet, 65535);
            REQUIRE(accept(0) == PacketStatus::Valid && !gap);
            SetSequence(packet, 0);
            REQUIRE(accept(5000) == PacketStatus::Valid && !gap);
            break;
        case 12:
            SetSequence(packet, 31415);
            REQUIRE(accept(0) == PacketStatus::Valid && !gap);
            break;
        case 13:
            REQUIRE(accept(0) == PacketStatus::Valid);
            SetSequence(packet, 0);
            REQUIRE(accept(5000) == PacketStatus::Valid && gap);
            break;
        case 14:
        {
            LoRaPayload_t sentinels = {1, 1, -9990, 0, 0, 0, -9990, 999, 0, 0};
            sentinels.crc8 = crc8_calc((uint8_t *)&sentinels, 17);
            memcpy(packet, &sentinels, 18);
            REQUIRE(accept(0) == PacketStatus::Valid);
            REQUIRE(output.temperature_x10 == -9990 && output.wind_speed_x100 == -9990 &&
                    output.wind_dir_deg == 999);
            break;
        }
        case 15:
            REQUIRE(accept(0) == PacketStatus::Valid);
            REQUIRE(accept(15000) == PacketStatus::Valid && gap);
            REQUIRE(state.last_valid_ms == 15000 && !GatewayCheckSilence(state, 20000));
            break;
        case 16:
            REQUIRE(!GatewayCheckSilence(state, 19999));
            REQUIRE(GatewayCheckSilence(state, 20000));
            REQUIRE(!GatewayCheckSilence(state, 20001));
            break;
        case 17:
            REQUIRE(accept(5000) == PacketStatus::Valid);
            REQUIRE(!GatewayCheckSilence(state, 24999));
            REQUIRE(GatewayCheckSilence(state, 25000));
            break;
        case 18:
            packet[17] ^= 1;
            for (uint32_t t = 0; t < 20000; t += 1000)
            {
                REQUIRE(accept(t) == PacketStatus::Crc);
                REQUIRE(!GatewayCheckSilence(state, t));
            }
            REQUIRE(GatewayCheckSilence(state, 20000));
            break;
        case 19:
            REQUIRE(GatewayCheckSilence(state, 20000));
            for (uint32_t t = 20001; t < 25000; ++t)
            {
                REQUIRE(!GatewayCheckSilence(state, t));
            }
            break;
        case 20:
            REQUIRE(GatewayCheckSilence(state, 20000));
            packet[17] ^= 1;
            REQUIRE(accept(21000) == PacketStatus::Crc && state.silent && !recovered);
            packet[17] ^= 1;
            REQUIRE(accept(22000) == PacketStatus::Valid && recovered && !state.silent);
            REQUIRE(accept(23000) == PacketStatus::Valid && !recovered);
            REQUIRE(!GatewayCheckSilence(state, 42999));
            REQUIRE(GatewayCheckSilence(state, 43000));
            break;
        case 21:
        {
            uint32_t start = UINT32_MAX - 1000u;
            GatewayStart(state, start);
            REQUIRE(!GatewayCheckSilence(state, (uint32_t)(start + 19999u)));
            REQUIRE(GatewayCheckSilence(state, (uint32_t)(start + 20000u)));
            break;
        }
        case 22:
        {
            FakeRadio radio(18);
            memcpy(radio.bytes.data(), golden_packet, 18);
            uint8_t buf[20];
            memset(buf, 0xa5, sizeof buf);
            REQUIRE(LoRaReadPacket(radio, buf + 1, 18) == 18);
            REQUIRE(memcmp(buf + 1, golden_packet, 18) == 0 && buf[0] == 0xa5 && buf[19] == 0xa5);
            FakeRadio empty(0);
            REQUIRE(LoRaReadPacket(empty, buf, 18) == -1);
            break;
        }
        case 23:
        {
            FakeRadio radio(255);
            radio.stuck = true;
            REQUIRE(LoRaReadPacket(radio, packet, 18) == 0 && radio.index == 255);
            FakeRadio extra(18);
            extra.stuck = true;
            REQUIRE(LoRaReadPacket(extra, packet, 18) == 0 && extra.index == 255);
            FakeRadio small(18);
            REQUIRE(LoRaReadPacket(small, packet, 17) == 0);
            break;
        }
        default:
            return 2;
    }
    printf("PASS gateway G%02d\n", test);
    return 0;
}
