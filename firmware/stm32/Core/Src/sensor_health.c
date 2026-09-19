/**
 * @file sensor_health.c
 * @brief Implementa as transições de saúde observadas por sensor.
 */

#include "sensor_health.h"

HealthEvent SensorHealth_Update(SensorHealth *h, SensorReason reason, uint32_t now)
{
    HealthEvent event = HEALTH_NONE;
    if (!h->observed)
    {
        event = HEALTH_FIRST;
    }
    else if (reason == SENSOR_OK && !h->valid)
    {
        event = HEALTH_RECOVERED;
    }
    else if (reason != SENSOR_OK && h->valid)
    {
        event = HEALTH_FAILURE;
    }
    else if (reason != h->reason)
    {
        event = HEALTH_REASON_CHANGED;
    }
    h->observed = true;
    h->valid = reason == SENSOR_OK;
    h->reason = reason;
    if (h->valid)
    {
        h->has_valid = true;
        h->last_valid_ms = now;
        h->consecutive_failures = 0;
    }
    else
    {
        if (h->consecutive_failures < UINT8_MAX)
        {
            ++h->consecutive_failures;
        }
        if (h->consecutive_failures == SENSOR_FAILURE_THRESHOLD)
        {
            event = HEALTH_PERSISTENT;
        }
    }
    return event;
}

const char *Sensor_Name(SensorId id)
{
    static const char *const names[] = {"DHT22", "BMP280", "BH1750", "BIRUTA", "ANEM", "PLUV"};
    return (unsigned)id < SENSOR_COUNT ? names[id] : "DESCONHECIDO";
}
const char *Sensor_Reason(SensorReason reason)
{
    static const char *const names[] = {"OK",           "NAO_INICIALIZADO", "COMUNICACAO_ADC",
                                        "TIMEOUT",      "CHECKSUM",         "FAIXA_INVALIDA",
                                        "CONFIGURACAO", "SATURACAO",        "TIMER",
                                        "CALIBRACAO"};
    return (unsigned)reason < sizeof(names) / sizeof(names[0]) ? names[reason] : "DESCONHECIDO";
}
const char *Sensor_Event(HealthEvent event)
{
    static const char *const names[] = {"SEM_MUDANCA",     "PRIMEIRA_LEITURA", "FALHA",
                                        "MOTIVO_ALTERADO", "PERSISTENTE",      "RECUPERADO"};
    return (unsigned)event < sizeof(names) / sizeof(names[0]) ? names[event] : "DESCONHECIDO";
}
