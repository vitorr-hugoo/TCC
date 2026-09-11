/**
 * @file    gateway_esp32_lora.ino
 * @brief   Gateway receptor LoRa P2P — ESP32 WROOM-32 DevKitC V1 + SX1276
 * @author  TCC — Estação Meteorológica IoT
 * @version 1.0.0 (fase bancada — sem MQTT)
 *
 *
 * Pinagem (VSPI ESP32 WROOM-32):
 *   MOSI  → GPIO 23
 *   MISO  → GPIO 19
 *   SCK   → GPIO 18
 *   NSS   → GPIO  5   (CS)
 *   RESET → GPIO 14
 *   DIO0  → GPIO 26   (IRQ RX_Done)
 *
 * Configuração LoRa (idêntica ao transmissor STM32):
 *   Frequência : 915 MHz
 *   SF         : 9
 *   BW         : 125 kHz
 *   CR         : 4/5
 *   Preâmbulo  : 8 símbolos
 *   CRC modem  : habilitado
 *   Header     : explícito (implicit=false)
 *
 * Evolução planejada:
 *   v1.x  → Wi-Fi + MQTT
 *   v2.x  → JSON payload + InfluxDB
 */

#include <SPI.h>
#include <LoRa.h>
#include <esp_task_wdt.h>
#include <WiFi.h>
#include <PubSubClient.h>

/* ─────────────────────────────────────────────
   1. PINAGEM
   ───────────────────────────────────────────── */
#define LORA_NSS    5
#define LORA_RST   14
#define LORA_DIO0  26
/* MOSI=23, MISO=19, SCK=18 são fixos no VSPI do WROOM-32 */

/* ─────────────────────────────────────────────
   CONFIGURAÇÕES WI-FI E MQTT
   ───────────────────────────────────────────── */
#define WIFI_SSID        "2,4 CHAMA NOIS 22"
#define WIFI_PASSWORD    "xupafederupa"
#define MQTT_BROKER_IP   "192.168.0.83"
//#define MQTT_BROKER_IP   "192.168.0.83"    // IP do seu PC anotado no ipconfig
#define MQTT_PORT        1883
#define MQTT_CLIENT_ID   "gateway_esp32_lora"
#define MQTT_TOPIC       "estacao/no1/dados"
#define MQTT_RECONNECT_INTERVAL_MS  5000  // tenta reconectar a cada 5s

/* ─────────────────────────────────────────────
   2. PARÂMETROS DE RÁDIO — devem espelhar o TX
   ───────────────────────────────────────────── */
#define LORA_FREQ        915E6   /* 915 MHz                    */
#define LORA_SF          9       /* Spreading Factor 9         */
#define LORA_BW          125E3   /* Bandwidth 125 kHz          */
#define LORA_CR          5       /* Coding Rate 4/5 → valor=5  */
#define LORA_PREAMBLE    8       /* Símbolos de preâmbulo      */
#define LORA_TX_POWER    17      /* Referência; RX ignora      */

/* ─────────────────────────────────────────────
   3. PROTOCOLO DE PAYLOAD
   ───────────────────────────────────────────── */
#define EXPECTED_PAYLOAD_LEN  18   /* sizeof(LoRaPayload_t) = 18 B */

/* Sentinelas definidos pelo transmissor */
#define SENTINEL_TEMP_INVALID    (-9990)
#define SENTINEL_WIND_INVALID    (-9990)
#define SENTINEL_DIR_INVALID     (999)
#define SENTINEL_HUMID_INVALID   (0)
#define SENTINEL_PRES_INVALID    (0)
#define SENTINEL_LUX_INVALID     (0)


#define WDT_TIMEOUT_S  30

// ── Diagnóstico de enlace ─────────────────────
static uint32_t diag_pkt_received  = 0;  // total de pacotes recebidos pelo rádio
static uint32_t diag_pkt_size_err  = 0;  // descartados por tamanho incorreto
static uint32_t diag_pkt_crc_err   = 0;  // reprovados no CRC-8 de aplicação
static uint32_t diag_pkt_seq_gap   = 0;  // gaps detectados na sequência
static uint16_t diag_last_seq      = 0;  // último número de sequência válido visto
static bool     diag_first_packet  = true; // flag para ignorar o primeiro gap

/* ─────────────────────────────────────────────
   OBJETOS WI-FI E MQTT
   ───────────────────────────────────────────── */
WiFiClient   wifi_client;
PubSubClient mqtt_client(wifi_client);

/* ─────────────────────────────────────────────
   4. STRUCT DO PAYLOAD (idêntica ao transmissor)
      #pragma pack não existe no GCC do ESP32,
      mas __attribute__((packed)) equivale.
   ───────────────────────────────────────────── */
typedef struct __attribute__((packed)) {
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

/* ─────────────────────────────────────────────
   5. PROTÓTIPOS
   ───────────────────────────────────────────── */
bool     lora_init(void);
int      lora_receive_packet(uint8_t *buf, int buf_size, int *rssi, float *snr);
bool     payload_validate_crc(const LoRaPayload_t *p);
void     payload_decode(const uint8_t *raw, LoRaPayload_t *p);
void     payload_print(const LoRaPayload_t *p, int rssi, float snr, bool crc_ok);
uint8_t  crc8_calc(const uint8_t *data, uint16_t len);
void     diag_print_summary(void);
void wifi_connect(void);
bool mqtt_ensure_connected(void);
void mqtt_publish_payload(const LoRaPayload_t *p, int rssi, float snr, bool crc_ok);

/* ═══════════════════════════════════════════════
   SETUP
   ═══════════════════════════════════════════════ */
void setup(void)
{
    Serial.begin(115200);
    while (!Serial && millis() < 3000);   /* aguarda Serial (debug USB) */

    Serial.println(F("\n======================================"));
    Serial.println(F("  Gateway LoRa RX — ESP32 + SX1276"));
    Serial.println(F("======================================"));

    if (!lora_init()) {
        Serial.println(F("[ERRO FATAL] Falha na inicializacao do SX1276."));
        while (true) { 
            delay(1000); 
        }   /* para execução — inspecionar fiação */
    }

    Serial.println(F("[GATEWAY] Aguardando pacotes LoRa...\n"));

    // WDT — API correta para ESP32 Arduino Core 3.x
    /*const esp_task_wdt_config_t wdt_cfg = {
        .timeout_ms     = 30000,  // 30 segundos em milissegundos
        .idle_core_mask = 0,      // não monitora cores ociosos
        .trigger_panic  = true    // true = reseta ao estourar
    };
    */
     esp_task_wdt_init(WDT_TIMEOUT_S, true);  // true = reset ao estourar (panic)
    esp_task_wdt_add(NULL);                  // registra a task atual (loopTask)
    Serial.println(F("[WDT] Watchdog configurado: timeout = 30s"));
    
    wifi_connect();
    mqtt_client.setServer(MQTT_BROKER_IP, MQTT_PORT);
    mqtt_client.setBufferSize(512);
    delay(1000);
    mqtt_ensure_connected();
    
    if (mqtt_client.connected()) {
        bool ok = mqtt_client.publish("estacao/teste", "{\"msg\":\"boot esp32\"}");
        Serial.print(F("[MQTT] Publish de teste no setup: "));
        Serial.println(ok ? F("OK") : F("FALHOU"));
    } else {
        Serial.println(F("[MQTT] Sem conexao no setup, teste nao enviado."));
    }

    Serial.println(F("[DEBUG] setup() terminou."));
}

/* ═══════════════════════════════════════════════
   LOOP — recepção contínua não-bloqueante
   ═══════════════════════════════════════════════ */
void loop(void)
{
    esp_task_wdt_reset();

    if (WiFi.status() == WL_CONNECTED) {
    if (!mqtt_client.connected()) {
        mqtt_ensure_connected();
    } else {
        mqtt_client.loop();
    }
}

    uint8_t raw[EXPECTED_PAYLOAD_LEN];
    int     rssi = 0;
    float   snr  = 0.0f;

    int rx_len = lora_receive_packet(raw, sizeof(raw), &rssi, &snr);

    if (rx_len < 0) {
        delay(10);
        return;
    }

    // ── conta tudo que chegou no rádio, independente de validade
    diag_pkt_received++;

    Serial.print(F("\n--- Pacote #"));
    Serial.print(diag_pkt_received);
    Serial.print(F("  len="));
    Serial.print(rx_len);
    Serial.println(F(" ---"));

    // ── validação de tamanho
    if (rx_len != EXPECTED_PAYLOAD_LEN) {
        diag_pkt_size_err++;
        Serial.print(F("[AVISO] Tamanho inesperado: "));
        Serial.print(rx_len);
        Serial.print(F(" (esperado "));
        Serial.print(EXPECTED_PAYLOAD_LEN);
        Serial.print(F("). Descartado. Total erros tamanho: "));
        Serial.println(diag_pkt_size_err);
        return;
    }

    // ── decodificação
    LoRaPayload_t payload;
    payload_decode(raw, &payload);

    // ── detecção de gap de sequência
    // Ignora a comparação no primeiro pacote recebido (não há "anterior")
    if (!diag_first_packet) {
        uint16_t expected_seq = diag_last_seq + 1;
        if (payload.seq != expected_seq) {
            diag_pkt_seq_gap++;
            Serial.print(F("[AVISO] Gap de sequencia: esperado "));
            Serial.print(expected_seq);
            Serial.print(F(", recebido "));
            Serial.print(payload.seq);
            Serial.print(F(". Total gaps: "));
            Serial.println(diag_pkt_seq_gap);
        }
    }
    diag_first_packet = false;
    diag_last_seq = payload.seq;

    // ── validação de CRC
    bool crc_ok = payload_validate_crc(&payload);
    if (!crc_ok) {
        diag_pkt_crc_err++;
    }

    // ── impressão dos dados + resumo de diagnóstico
    payload_print(&payload, rssi, snr, crc_ok);
    
    // Publica apenas se CRC ok — não envia dado corrompido ao banco
    if (crc_ok) {
        mqtt_publish_payload(&payload, rssi, snr, crc_ok);
    } else {
        Serial.println(F("[MQTT] Publicação suprimida — CRC falhou."));
    }

    diag_print_summary();
}

/* ═══════════════════════════════════════════════
   IMPLEMENTAÇÕES DAS FUNÇÕES
   ═══════════════════════════════════════════════ */

/**
 * @brief  Inicializa o SX1276 via biblioteca LoRa (Sandeepmistry).
 * @return true  se bem-sucedido, false em falha.
 *
 * Riscos:
 *  - Fiação invertida (MOSI/MISO trocados) → LoRa.begin() retorna 0
 *  - NSS no GPIO correto mas sem pull-up em MISO → leitura instável
 *  - Módulo 433 MHz tentando operar em 915 MHz → begin() falha ou RX nulo
 */
bool lora_init(void)
{
    /* Informa pinos não-padrão à biblioteca antes de .begin() */
    LoRa.setPins(LORA_NSS, LORA_RST, LORA_DIO0);

    Serial.print(F("[LORA] Inicializando SX1276 @ "));
    Serial.print((long)LORA_FREQ / 1000000L);
    Serial.println(F(" MHz..."));

    if (!LoRa.begin(LORA_FREQ)) {
        Serial.println(F("[LORA] FALHA: LoRa.begin() retornou 0."));
        Serial.println(F("       Verificar: fiacao SPI, NSS/RST/DIO0, alimentacao 3.3V."));
        return false;
    }

    /* ── parâmetros idênticos ao transmissor STM32 ── */
    LoRa.setSpreadingFactor(LORA_SF);          /* SF9               */
    LoRa.setSignalBandwidth(LORA_BW);          /* 125 kHz           */
    LoRa.setCodingRate4(LORA_CR);              /* 4/5               */
    LoRa.setPreambleLength(LORA_PREAMBLE);     /* 8 símbolos        */
    LoRa.enableCrc();                          /* CRC modem ON      */
    /* header explícito é o padrão da biblioteca — não há disableImplicitHeader() a chamar */

    /* Modo recepção contínua */
    LoRa.receive();

    Serial.println(F("[LORA] Init OK."));
    Serial.print(F("       SF="));   Serial.print(LORA_SF);
    Serial.print(F(" BW=125kHz CR=4/5 CRC=ON Preambulo="));
    Serial.print(LORA_PREAMBLE);
    Serial.println(F(" simbolos"));

    return true;
}

/**
 * @brief  Verifica se há pacote disponível e o lê para o buffer.
 * @param  buf       Buffer de destino.
 * @param  buf_size  Tamanho máximo do buffer.
 * @param  rssi      [out] RSSI em dBm.
 * @param  snr       [out] SNR em dB.
 * @return Número de bytes lidos, ou -1 se nenhum pacote disponível.
 *
 * Nota: LoRa.parsePacket() usa polling — não bloqueia.
 *       Para uso com interrupção em DIO0, ver nota de evolução abaixo.
 */
int lora_receive_packet(uint8_t *buf, int buf_size, int *rssi, float *snr)
{
    int pkt_size = LoRa.parsePacket();
    if (pkt_size == 0) {
        return -1;   /* nenhum pacote */
    }

    /* Lê bytes disponíveis até buf_size */
    int idx = 0;
    while (LoRa.available() && idx < buf_size) {
        buf[idx++] = (uint8_t)LoRa.read();
    }

    /* Drena eventuais bytes excedentes para não corromper próxima recepção */
    while (LoRa.available()) {
        (void)LoRa.read();
    }

    *rssi = LoRa.packetRssi();
    *snr  = LoRa.packetSnr();

    return idx;   /* bytes efetivamente lidos */
}

/**
 * @brief  Copia raw bytes para a struct usando memcpy (seguro para packed).
 *
 * ATENÇÃO — Endianness:
 *   STM32F4 e ESP32 são ambos Little-Endian (ARM Cortex-M4 e Xtensa LX6).
 *   Portanto, campos uint16_t/int16_t são transmitidos e recebidos
 *   corretamente sem swapping. Registrar esse fato no TCC.
 */
void payload_decode(const uint8_t *raw, LoRaPayload_t *p)
{
    memcpy(p, raw, sizeof(LoRaPayload_t));
}

/**
 * @brief  Calcula CRC-8 (polinômio 0x07, init 0x00) sobre os primeiros
 *         sizeof(LoRaPayload_t)-1 bytes e compara com p->crc8.
 *
 * Algoritmo idêntico ao do transmissor STM32 (App_CRC8).
 */
bool payload_validate_crc(const LoRaPayload_t *p)
{
    uint8_t calc = crc8_calc((const uint8_t *)p, sizeof(LoRaPayload_t) - 1);
    return (calc == p->crc8);
}

/**
 * @brief  CRC-8 — polinômio 0x07, init 0x00.
 *         Bit-a-bit, MSB primeiro. Idêntico à App_CRC8() do transmissor.
 */
uint8_t crc8_calc(const uint8_t *data, uint16_t len)
{
    uint8_t crc = 0x00u;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t b = 0; b < 8u; b++) {
            if (crc & 0x80u)
                crc = (uint8_t)((crc << 1) ^ 0x07u);
            else
                crc <<= 1;
        }
    }
    return crc;
}

/**
 * @brief  Imprime os campos decodificados no Serial Monitor.
 *
 * Convenções de escala (idem transmissor):
 *   temperature_x10  → divide por 10.0   (ex: 253 → 25.3 °C)
 *   humidity_x10     → divide por 10.0   (ex: 650 → 65.0 %)
 *   wind_speed_x100  → divide por 100.0  (ex: 350 → 3.50 m/s)
 *   rain_mm_x100     → divide por 100.0  (ex: 120 → 1.20 mm)
 *   pressure e lux   → inteiros diretos
 */
void payload_print(const LoRaPayload_t *p, int rssi, float snr, bool crc_ok)
{
    Serial.println(F("┌─────────────────────────────────────┐"));
    Serial.println(F("             PAYLOAD LORA              "));
    Serial.println(F("└─────────────────────────────────────┘"));

    /* ── identificação ── */
    Serial.print(F("  Node ID  : 0x"));
    Serial.println(p->node_id, HEX);
    Serial.print(F("  Seq      : "));
    Serial.println(p->seq);

    /* ── temperatura ── */
    Serial.print(F("  Temp     : "));
    if (p->temperature_x10 == SENTINEL_TEMP_INVALID) {
        Serial.println(F("INVALIDA (sensor sem resposta)"));
    } else {
        Serial.print(p->temperature_x10 / 10.0f, 1);
        Serial.println(F(" °C"));
    }

    /* ── umidade ── */
    Serial.print(F("  Umidade  : "));
    if (p->humidity_x10 == SENTINEL_HUMID_INVALID) {
        Serial.println(F("INVALIDA"));
    } else {
        Serial.print(p->humidity_x10 / 10.0f, 1);
        Serial.println(F(" %"));
    }

    /* ── pressão ── */
    Serial.print(F("  Pressao  : "));
    if (p->pressure_hpa_x1 == SENTINEL_PRES_INVALID) {
        Serial.println(F("INVALIDA"));
    } else {
        Serial.print(p->pressure_hpa_x1);
        Serial.println(F(" hPa"));
    }

    /* ── iluminância ── */
    Serial.print(F("  Ilum     : "));
    if (p->illuminance_lux_x1 == SENTINEL_LUX_INVALID) {
        Serial.println(F("INVALIDA"));
    } else {
        Serial.print(p->illuminance_lux_x1);
        Serial.println(F(" lux"));
    }

    /* ── vento ── */
    Serial.print(F("  Vento    : "));
    if (p->wind_speed_x100 == SENTINEL_WIND_INVALID) {
        Serial.println(F("INVALIDA"));
    } else {
        Serial.print(p->wind_speed_x100 / 100.0f, 2);
        Serial.println(F(" m/s"));
    }

    /* ── direção do vento ── */
    Serial.print(F("  Direcao  : "));
    if (p->wind_dir_deg == SENTINEL_DIR_INVALID) {
        Serial.println(F("INVALIDA"));
    } else {
        Serial.print(p->wind_dir_deg);
        Serial.println(F(" graus"));
    }

    /* ── chuva ── */
    Serial.print(F("  Chuva    : "));
    Serial.print(p->rain_mm_x100 / 100.0f, 2);
    Serial.println(F(" mm"));

    /* ── telemetria de enlace ── */
    Serial.println(F("  ─────────────────────────────────── "));
    Serial.print(F("  RSSI     : "));
    Serial.print(rssi);
    Serial.println(F(" dBm"));
    Serial.print(F("  SNR      : "));
    Serial.print(snr, 1);
    Serial.println(F(" dB"));

    /* ── CRC da aplicação ── */
    Serial.print(F("  CRC-8    : "));
    if (crc_ok) {
        Serial.println(F("OK"));
    } else {
        Serial.print(F("FALHOU (rx=0x"));
        Serial.print(p->crc8, HEX);
        Serial.print(F(", calc=0x"));
        Serial.print(crc8_calc((const uint8_t *)p, sizeof(LoRaPayload_t) - 1), HEX);
        Serial.println(F(")"));
    }

    Serial.println();
}

void diag_print_summary(void)
{
    // Evita divisão por zero na primeira recepção
    float pdr = 0.0f;
    if (diag_pkt_received > 0) {
        uint32_t pkt_ok = diag_pkt_received - diag_pkt_size_err - diag_pkt_crc_err;
        pdr = (float)pkt_ok / (float)diag_pkt_received * 100.0f;
    }

    Serial.println(F("  ─── Diagnóstico de Enlace ───────────"));
    Serial.print(F("  Recebidos : ")); Serial.println(diag_pkt_received);
    Serial.print(F("  Erro tam. : ")); Serial.println(diag_pkt_size_err);
    Serial.print(F("  Erro CRC  : ")); Serial.println(diag_pkt_crc_err);
    Serial.print(F("  Gaps seq  : ")); Serial.println(diag_pkt_seq_gap);
    Serial.print(F("  PDR aprox.: ")); Serial.print(pdr, 1); Serial.println(F(" %"));
    Serial.println();
}

/* ══════════════════════════════════════════════
   WI-FI
   ══════════════════════════════════════════════ */
void wifi_connect(void)
{
    Serial.print(F("[WiFi] Conectando a "));
    Serial.print(F(WIFI_SSID));
    Serial.print(F("..."));

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    uint8_t attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(F("."));
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println(F(" OK"));
        Serial.print(F("[WiFi] IP do gateway: "));
        Serial.println(WiFi.localIP());
    } else {
        Serial.println(F("\n[WiFi] FALHA ao conectar. Verifique SSID/senha."));
        Serial.println(F("[WiFi] Gateway operará só em modo local (Serial)."));
        // Não trava o sistema — LoRa continua funcionando sem Wi-Fi
    }
}

/* ══════════════════════════════════════════════
   MQTT — reconexão não-bloqueante
   ══════════════════════════════════════════════ */
/*bool mqtt_ensure_connected(void)
{
    // Se já conectado, nada a fazer
    if (mqtt_client.connected()) return true;

    // Sem Wi-Fi não há o que tentar
    if (WiFi.status() != WL_CONNECTED) return false;

    static uint32_t last_attempt_ms = 0;
    uint32_t now = millis();

    // Respeita o intervalo entre tentativas para não bloquear o loop
    if (now - last_attempt_ms < MQTT_RECONNECT_INTERVAL_MS) return false;
    last_attempt_ms = now;

    Serial.print(F("[MQTT] Conectando ao broker "));
    Serial.print(F(MQTT_BROKER_IP));
    Serial.print(F("..."));

    if (mqtt_client.connect(MQTT_CLIENT_ID)) {
        Serial.println(F(" OK"));
        return true;
    }

    Serial.print(F(" FALHOU. rc="));
    Serial.println(mqtt_client.state());
    return false;
}*/

bool mqtt_ensure_connected(void)
{
    if (mqtt_client.connected()) {
        Serial.println(F("[MQTT] Ja conectado."));
        return true;
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println(F("[MQTT] Wi-Fi nao conectado."));
        return false;
    }

    static uint32_t last_attempt_ms = 0;
    uint32_t now = millis();

    if (now - last_attempt_ms < MQTT_RECONNECT_INTERVAL_MS) {
        return false;
    }
    last_attempt_ms = now;

    Serial.print(F("[MQTT] Tentando conectar em "));
    Serial.print(MQTT_BROKER_IP);
    Serial.print(F(":"));
    Serial.println(MQTT_PORT);

    Serial.print(F("[MQTT] state() antes = "));
    Serial.println(mqtt_client.state());

    //bool ok = mqtt_client.connect(MQTT_CLIENT_ID);
    String clientId = String(MQTT_CLIENT_ID) + "_" + String((uint32_t)ESP.getEfuseMac(), HEX);
    bool ok = mqtt_client.connect(clientId.c_str());


    Serial.print(F("[MQTT] state() depois = "));
    Serial.println(mqtt_client.state());

    if (ok) {
        Serial.println(F("[MQTT] Conexao OK."));
        return true;
    }

    Serial.println(F("[MQTT] Falhou ao conectar."));
    return false;
}

/* ══════════════════════════════════════════════
   MQTT — publicação em JSON
   ══════════════════════════════════════════════ */
void mqtt_publish_payload(const LoRaPayload_t *p, int rssi, float snr, bool crc_ok)
{
    if (!mqtt_client.connected()) {
        Serial.println(F("[MQTT] Cliente nao conectado — publicacao ignorada."));
        return;
    }

    // Monta JSON manualmente — evita dependência da ArduinoJson nessa fase
    char json[400];
    snprintf(json, sizeof(json),
        "{"
        "\"node_id\":%u,"
        "\"seq\":%u,"
        "\"temp_c\":%.1f,"
        "\"hum_pct\":%.1f,"
        "\"pres_hpa\":%u,"
        "\"lux\":%u,"
        "\"wind_ms\":%.2f,"
        "\"wind_deg\":%u,"
        "\"rain_mm\":%.2f,"
        "\"rssi\":%d,"
        "\"snr\":%.1f,"
        "\"crc_ok\":%s"
        "}",
        p->node_id,
        p->seq,
        p->temperature_x10  / 10.0f,
        p->humidity_x10     / 10.0f,
        p->pressure_hpa_x1,
        p->illuminance_lux_x1,
        p->wind_speed_x100  / 100.0f,
        p->wind_dir_deg,
        p->rain_mm_x100     / 100.0f,
        rssi,
        snr,
        crc_ok ? "true" : "false"
    );

    bool ok = mqtt_client.publish(MQTT_TOPIC, json);

    Serial.print(F("[MQTT] Publish "));
    Serial.print(ok ? F("OK") : F("FALHOU"));
    //Serial.print(F(" → "));
    //Serial.println(MQTT_TOPIC);
}
/* ─────────────────────────────────────────────────────────────────────────────
   NOTA DE EVOLUÇÃO v1.x — MQTT
   ─────────────────────────────────────────────────────────────────────────────
   Para adicionar MQTT:
     1. Incluir WiFi.h e PubSubClient.h (Nick O'Leary)
     2. Adicionar wifi_connect() e mqtt_connect() em setup()
     3. Chamar mqtt_publish_payload() no loop() após payload_print()
     4. Formato JSON sugerido:
          {
            "node": 1, "seq": 42,
            "temp_c": 25.3, "hum_pct": 65.0,
            "pres_hpa": 1013, "lux": 850,
            "wind_ms": 3.50, "wind_deg": 270,
            "rain_mm": 1.20,
            "rssi": -85, "snr": 9.5, "crc_ok": true
          }
   ───────────────────────────────────────────────────────────────────────────── */
