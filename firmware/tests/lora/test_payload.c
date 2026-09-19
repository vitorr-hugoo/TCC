#include "lora_payload.h"
#include "fixtures.h"
#include <string.h>
#include <stdio.h>

int main(void)
{
    if (App_CRC8((const uint8_t *)"123456789", 9) != 0xf4 || App_CRC8(golden_packet, 17) != 0x3f)
    {
        return 1;
    }
    LoRaPayload_t p = {1, 42, 250, 600, 1013, 1000, 250, 180, 123, 0};
    p.crc8 = App_CRC8((const uint8_t *)&p, sizeof p - 1);
    if (memcmp(&p, golden_packet, sizeof p))
    {
        return 1;
    }
    puts("PASS STM32 CRC de referencia e bytes exatos do pacote");
    return 0;
}
