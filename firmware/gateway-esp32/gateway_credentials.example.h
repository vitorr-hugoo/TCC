/**
 * @file gateway_credentials.example.h
 * @brief Modelo local de credenciais do gateway ESP32.
 *
 * Copie este arquivo como gateway_credentials.h e preencha os valores do
 * ambiente de execução. O arquivo com valores reais é ignorado pelo Git.
 */

#define WIFI_SSID      "YOUR_WIFI_SSID"
#define WIFI_PASSWORD  "YOUR_WIFI_PASSWORD"
#define MQTT_BROKER_IP "192.0.2.1"
#define MQTT_PORT      1883U
#define MQTT_CLIENT_ID "weather-station-gateway"
#define MQTT_TOPIC     "weather-station/telemetry"
