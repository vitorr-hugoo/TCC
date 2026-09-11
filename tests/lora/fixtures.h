#ifndef LORA_TEST_FIXTURES_H
#define LORA_TEST_FIXTURES_H
#include <stdint.h>
/* CRC 0x3f conferido por divisao polinomial independente (polinomio 0x107).
 * no=1, seq=42, temp=25C, umid=60%, pressao=1013hPa, lux=1000,
 * vento=2.50m/s, direcao=180, chuva=1.23mm. */
static const uint8_t golden_packet[18] = {
    0x01,0x2a,0x00,0xfa,0x00,0x58,0x02,0xf5,0x03,
    0xe8,0x03,0xfa,0x00,0xb4,0x00,0x7b,0x00,0x3f
};
#endif
