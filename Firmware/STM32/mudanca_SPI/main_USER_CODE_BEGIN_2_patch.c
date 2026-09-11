/* ============================================================
 * PATCH: bloco USER CODE BEGIN 2 do main.c
 *
 * Substitua TODO o conteúdo entre
 *   "/* USER CODE BEGIN 2 *\/"
 * e
 *   "/* USER CODE END 2 *\/"
 * pelo código abaixo.
 *
 * MUDANÇAS em relação à versão anterior:
 *  1. SD_Deselect() inicializa apenas o CS do SD — correto.
 *  2. LORA_NSS é posto em HIGH explicitamente antes de qualquer
 *     chamada LoRa (boa prática, evita NSS flutuante no boot).
 *  3. Ordem de chamada: MX_SPI2_Init() já foi chamado antes
 *     (pelo CubeMX em main), então hspi2 está pronto aqui.
 *  4. Diagnóstico estendido: se 0xFF → imprime dica de hardware.
 * ============================================================ */

/* USER CODE BEGIN 2 */

/* ── Estado inicial dos barramentos ──────────────────────────────────────
 * SPI1 (SD)  : CS = HIGH (SD_Deselect)
 * SPI2 (LoRa): NSS = HIGH (escrita direta no pino)
 * Garante que nenhum periférico está selecionado durante o boot.
 */
SD_Deselect();
HAL_GPIO_WritePin(LORA_NSS_GPIO_Port, LORA_NSS_Pin, GPIO_PIN_SET);

/* ── Verificação do SX1276 via SPI2 ──────────────────────────────────────
 * SX1276_CheckVersion():
 *   1. Pulsa RESET (PB5) conforme datasheet §7.2.2.
 *   2. Lê REG_VERSION (0x42) via SPI2.
 *   3. Retorna true se 0x12 (rádio OK).
 *
 * Valores de diagnóstico:
 *   0x12 → OK
 *   0xFF → MISO solto ou NSS não abaixou (verificar hardware)
 *   0x00 → rádio sem alimentação ou SPI2 com problema de AF/GPIO
 */
uint8_t lora_version = 0x00u;

if (SX1276_CheckVersion(&lora_version))
{
    printf("[LORA] SX1276 OK. RegVersion = 0x%02X\r\n", lora_version);
}
else
{
    printf("[LORA] FALHA. RegVersion = 0x%02X\r\n", lora_version);

    if (lora_version == 0xFFu)
    {
        printf("[LORA] DICA: 0xFF = MISO em pull-up. Verifique:\r\n");
        printf("  1. Alimentacao 3.3V no modulo LoRa?\r\n");
        printf("  2. PB13/14/15 configurados como AF5 (SPI2) no CubeMX?\r\n");
        printf("  3. PB0 (LORA_NSS) configurado como Output PP?\r\n");
    }
    else if (lora_version == 0x00u)
    {
        printf("[LORA] DICA: 0x00 = radio sem resposta. Verifique alimentacao.\r\n");
    }
}

printf("\r\n=== Inicializando sensores ===\r\n");

/* DHT22 */
HAL_TIM_Base_Start(&htim10);
DHT22_Init(&dht22, DHT22_GPIO_Port, DHT22_Pin, &htim10);
printf("[DHT22] OK\r\n");

/* BMP280 */
BMP280_Config_t bmp_cfg = BMP280_DEFAULT_CONFIG;
BMP280_Status_t bmp_st  = BMP280_Init(&bmp280, &hi2c1, BMP280_ADDR_SDO_LOW, &bmp_cfg);
if (bmp_st == BMP280_OK)
    printf("[BMP280] OK - I2C1 0x76\r\n");
else
    printf("[BMP280] FALHA: %s\r\n", BMP280_StatusStr(bmp_st));

/* BH1750 */
BH1750_Status_t bh_st = BH1750_Init(&bh1750, &hi2c1, BH1750_ADDR_LOW,
                                      BH1750_MODE_CONT_H_RES);
if (bh_st == BH1750_OK)
    printf("[BH1750] OK - I2C1 0x23\r\n");
else
    printf("[BH1750] FALHA: %s\r\n", BH1750_StatusStr(bh_st));

/* Anemômetro */
anemometer_init();
printf("[ANEM] OK - EXTI PC7 + TIM6\r\n");

/* Pluviômetro */
pluviometer_init();
printf("[PLUV] OK - EXTI PC8\r\n");

/* Biruta */
App_WindVane_Init();

/* SD */
sd_ok = Storage_Init();
if (sd_ok)
    printf("[SD] OK - Cartao montado. Log em /logs/data.csv\r\n");
else
    printf("[SD] AVISO - Sem cartao ou falha na montagem. Continuando sem log.\r\n");

last_dht_status = DHT22_ERR_TIMEOUT;
last_bmp_status = BMP280_OK;
last_bh_status  = BH1750_OK;
last_dht_ms     = HAL_GetTick();

printf("Sistema Inicializado. A iniciar leituras...\r\n");

/* USER CODE END 2 */
