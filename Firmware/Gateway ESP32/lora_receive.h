#ifndef GATEWAY_LORA_RECEIVE_H
#define GATEWAY_LORA_RECEIVE_H
#include "lora_protocol.h"

/* Mesmo adaptador usado com LoRa real e fake nos testes.
 * -1: nenhum pacote; 0: tamanho/leitura invalida; 18: pacote completo.
 * Nunca transformar um prefixo valido de pacote maior em pacote aceito. */
template<class Radio>
int LoRaReadPacket(Radio &radio, uint8_t *buf, size_t capacity)
{
    int announced = radio.parsePacket();
    if (announced == 0) return -1;
    if (announced < 0 || announced > 255) return 0;
    bool valid = buf && capacity >= EXPECTED_PAYLOAD_LEN && announced == (int)EXPECTED_PAYLOAD_LEN;
    for (int i = 0; i < announced; ++i) {
        if (radio.available() <= 0) return 0;
        int byte = radio.read();
        if (byte < 0) return 0;
        if (valid) buf[i] = (uint8_t)byte;
    }
    if (radio.available() > 0) {
        /* Estado incoerente: descarte limitado ao maximo fisico do pacote. */
        for (int i = announced; i < 255 && radio.available() > 0; ++i)
            if (radio.read() < 0) break;
        return 0;
    }
    return valid ? (int)EXPECTED_PAYLOAD_LEN : 0;
}
#endif
