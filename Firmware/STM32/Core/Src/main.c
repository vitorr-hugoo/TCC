/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "fatfs.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>   // Necessário para manipulação de strings (strcpy, etc.)
#include "dht22.h"
#include "bmp280.h"
#include "bh1750.h"
#include "anemometer.h"
#include "pluviometer.h"
#include "wind_vane.h"
#include "storage.h"
#include "sd_spi.h"
#include "lora_sx1276.h"
#include "lora_payload.h"
#include "station_sample.h"

extern UART_HandleTypeDef huart2;

#ifdef __GNUC__
#define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
#define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#endif

PUTCHAR_PROTOTYPE
{
  HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
  return ch;
}
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

I2C_HandleTypeDef hi2c1;

SPI_HandleTypeDef hspi1;
SPI_HandleTypeDef hspi2;
DMA_HandleTypeDef hdma_spi1_rx;
DMA_HandleTypeDef hdma_spi1_tx;

TIM_HandleTypeDef htim6;
TIM_HandleTypeDef htim10;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

/* Variáveis para armazenar as leituras (ajuste os nomes se no seu código original estiverem diferentes) */
DHT22_Handle_t dht22;
BMP280_Handle_t bmp280;
BH1750_Handle_t bh1750;

uint32_t last_dht_ms = 0;

volatile uint32_t dbg_anem_exti_hits = 0;
volatile uint32_t dbg_anem_tim_hits  = 0;

/* --- NOVAS Variáveis para a Biruta --- */
wind_vane_config_t vane_cfg;
uint16_t wind_direction_deg = 0;
char wind_direction_label[8]; // Array para guardar os textos "N", "L", "S", "O"

/* Garante o acesso ao handle do ADC configurado pelo CubeMX */
extern ADC_HandleTypeDef hadc1;

static uint16_t lora_seq = 0u;

static bool acquisition_pending = false;
static uint32_t acquisition_start_ms = 0;
static SensorHealth sensor_health[SENSOR_COUNT];
static HAL_StatusTypeDef vane_status = HAL_ERROR;
static bool adc_initialized = false;
static uint8_t last_wire_overflow = 0;

static DHT22_Status_t last_dht_status;
static BMP280_Status_t last_bmp_status;
static BH1750_Status_t last_bh_status;

static Anemometer_Data_t last_dados_vento = {0};
static Pluviometer_Data_t last_dados_chuva = {0};


/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_I2C1_Init(void);
static void MX_TIM10_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM6_Init(void);
static void MX_ADC1_Init(void);
static void MX_SPI1_Init(void);
static void MX_SPI2_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/**
 * @brief LUT customizada para 4 posições — valores CALCULADOS.
 *
 * Reed  | Vout calc | ADC central | Direção atribuída
 * 560Ω  |  2,115 V  |    2627     | Norte   (0°)
 * 2kΩ   |  1,100 V  |    1365     | Leste   (90°)
 * 5kΩ   |  0,550 V  |     683     | Sul     (180°)
 * 10kΩ  |  0,300 V  |     373     | Oeste   (270°)

static const WindVaneLUT_t s_vane_lut[4u] = {
    // adc_min, adc_max, degrees, label
    {   2200u,   3000u,    0u, "N" },
    {   1265u,   1465u,   90u, "L" },
    {    583u,    783u,  180u, "S" },
    {    273u,    473u,  270u, "O" }
};
*/

// @brief LUT customizada para 4 posições

static const WindVaneLUT_t s_vane_lut[16u] = {
  {   0u,  299u, 113u, "ESE" },
  { 300u,  354u,  68u, "ENE" },
  { 355u,  439u,  90u, "E"   },
  { 440u,  622u, 158u, "SSE" },
  { 623u,  859u, 135u, "SE"  },
  { 860u, 1064u, 203u, "SSW" },
  { 1065u, 1386u, 180u, "S"   },
  { 1387u, 1734u,  23u, "NNE" },
  { 1735u, 2121u,  45u, "NE"  },
  { 2122u, 2459u, 248u, "WSW" },
  { 2460u, 2665u, 225u, "SW"  },
  { 2666u, 2977u, 338u, "NNW" },
  { 2978u, 3226u,   0u, "N"   },
  { 3227u, 3429u, 293u, "WNW" },
  { 3430u, 3664u, 315u, "NW"  },
  { 3665u, 4095u, 270u, "W"   }
};

static void App_WindVane_Init(void)
{
    vane_cfg.hadc               = &hadc1;
    vane_cfg.channel            = ADC_CHANNEL_0;
    vane_cfg.sampling_time      = ADC_SAMPLETIME_144CYCLES;
    vane_cfg.num_samples        = 8u;
    vane_cfg.sample_interval_ms = 5u;
    vane_cfg.north_offset_deg   = 0;
    vane_cfg.lut                = s_vane_lut; // ou WIND_VANE_DEFAULT_LUT para a definida no driver
    //vane_cfg.lut_size           = 4u;
    vane_cfg.lut_size           = 16u;

    vane_status = adc_initialized ? Wind_Vane_Init(&vane_cfg) : HAL_ERROR;
    printf("[BIRUTA] %s\r\n", vane_status == HAL_OK ? "ADC configurado; aguardando leitura" : "Falha de inicializacao do ADC");
}


static SensorReason App_DhtReason(DHT22_Status_t status)
{
    switch (status) {
        case DHT22_OK: return SENSOR_OK;
        case DHT22_ERR_TIMEOUT: return SENSOR_TIMEOUT;
        case DHT22_ERR_CHECKSUM: return SENSOR_CHECKSUM;
        case DHT22_ERR_TIMER: return SENSOR_TIMER;
        default: return SENSOR_RANGE;
    }
}
static SensorReason App_BmpReason(BMP280_Status_t status)
{
    switch (status) {
        case BMP280_OK: return SENSOR_OK;
        case BMP280_ERR_BUSY: return SENSOR_TIMEOUT;
        case BMP280_ERR_CONFIG: return SENSOR_CONFIG;
        case BMP280_ERR_CALIBRATION: return SENSOR_CALIBRATION;
        case BMP280_ERR_RANGE: return SENSOR_RANGE;
        default: return SENSOR_IO;
    }
}
static SensorReason App_BhReason(BH1750_Status_t status)
{
    switch (status) {
        case BH1750_OK: return SENSOR_OK;
        case BH1750_ERR_NOT_RDY: return SENSOR_TIMEOUT;
        case BH1750_ERR_CONFIG: return SENSOR_CONFIG;
        case BH1750_ERR_SATURATED: return SENSOR_SATURATED;
        case BH1750_ERR_RANGE: return SENSOR_RANGE;
        default: return SENSOR_IO;
    }
}
static float App_Value(const StationSample *s, SensorId id, float value)
{
    return s->reason[id] == SENSOR_OK ? value : -999.0f;
}
static void App_FinishAcquisition(uint32_t now, HAL_StatusTypeDef wind_status)
{
    StationSample sample = {
        .timestamp_ms = acquisition_start_ms,
        .reason = {
            App_DhtReason(last_dht_status), App_BmpReason(last_bmp_status),
            App_BhReason(last_bh_status),
            vane_status == HAL_OK ? SENSOR_OK : (vane_status == HAL_TIMEOUT ? SENSOR_TIMEOUT : SENSOR_IO),
            last_dados_vento.valid ? SENSOR_OK : (wind_status == HAL_TIMEOUT ? SENSOR_TIMEOUT : SENSOR_TIMER),
            last_dados_chuva.valid ? SENSOR_OK : SENSOR_RANGE
        },
        .temperature = dht22.temperature, .humidity = dht22.humidity,
        .pressure = bmp280.pressure, .bmp_temperature = bmp280.temperature, .altitude = bmp280.altitude,
        .illuminance = bh1750.illuminance, .irradiance = bh1750.irradiance,
        .wind_speed = last_dados_vento.speed_ms, .wind_dir_deg = wind_direction_deg,
        .rain_mm = last_dados_chuva.rain_mm
    };
    StationSample_Validate(&sample);
    FRESULT sd_result = FR_OK;
    for (unsigned i = 0; i < SENSOR_COUNT; ++i) {
        SensorHealth *h = &sensor_health[i];
        HealthEvent event = SensorHealth_Update(h, sample.reason[i], now);
        if (event == HEALTH_NONE) continue;
        printf("[SAUDE] %s %s | motivo=%s | falhas=%u | tem_ultima=%u | idade_ms=%lu\r\n",
            Sensor_Name((SensorId)i), Sensor_Event(event), Sensor_Reason(h->reason),
            h->consecutive_failures, h->has_valid,
            (unsigned long)(h->has_valid ? now - h->last_valid_ms : 0));
        if (Storage_IsReady()) {
            FRESULT res = Storage_AppendSensorHealth(now, (SensorId)i, h, event);
            if (res != FR_OK) sd_result = res;
        }
    }
    if (sample.reason[SENSOR_BMP] == SENSOR_IO && sample.reason[SENSOR_BH] == SENSOR_IO &&
        (sensor_health[SENSOR_BMP].consecutive_failures == 1 || sensor_health[SENSOR_BH].consecutive_failures == 1))
        printf("[I2C1] BMP280 e BH1750 falharam: verificar tambem barramento/alimentacao comuns.\r\n");

    SensorSample_t sd_sample = {
        .timestamp_ms = sample.timestamp_ms,
        .temperature = App_Value(&sample, SENSOR_DHT, sample.temperature),
        .humidity = App_Value(&sample, SENSOR_DHT, sample.humidity),
        .pressure = App_Value(&sample, SENSOR_BMP, sample.pressure),
        .irradiance = App_Value(&sample, SENSOR_BH, sample.irradiance),
        .wind_speed = App_Value(&sample, SENSOR_ANEM, sample.wind_speed),
        .wind_dir_deg = App_Value(&sample, SENSOR_VANE, sample.wind_dir_deg),
        .rain_mm = App_Value(&sample, SENSOR_RAIN, sample.rain_mm)
    };
    if (Storage_IsReady()) {
        FRESULT res = Storage_AppendSample(&sd_sample);
        if (res != FR_OK) sd_result = res;
    }
    LoRaPayload_t packet;
    uint8_t overflow = StationSample_Payload(&sample, lora_seq++, &packet);
    if (overflow != last_wire_overflow) {
        printf("[LORA] Limite do pacote legado: mascara=%02X. Campos sem representacao usam sentinela; dados locais preservados.\r\n", overflow);
        last_wire_overflow = overflow;
    }
    bool tx_ok = SX1276_Transmit((const uint8_t *)&packet, sizeof(packet));
    printf("[LORA] %s | seq=%u | len=%u | motivo=%s\r\n",
        tx_ok ? "TX concluida pelo radio" : "Falha local", packet.seq,
        (unsigned)sizeof(packet), SX1276_LastError());
    printf("\r\n===== DADOS DA ESTACAO | t=%lu ms =====\r\n", (unsigned long)sample.timestamp_ms);
    if (sample.reason[SENSOR_DHT] == SENSOR_OK)
        printf("Temp: %.1f C | Umid: %.1f %%\r\n", sample.temperature, sample.humidity);
    else printf("Temp/Umid: indisponiveis\r\n");
    if (sample.reason[SENSOR_BMP] == SENSOR_OK)
        printf("Pres: %.1f hPa | Alt: %.1f m\r\n", sample.pressure, sample.altitude);
    else printf("Pressao/Altitude: indisponiveis\r\n");
    if (sample.reason[SENSOR_BH] == SENSOR_OK)
        printf("Ilum: %.1f lux | Irrad: %.2f W/m2\r\n", sample.illuminance, sample.irradiance);
    else printf("Iluminancia/Irradiancia: indisponiveis\r\n");
    if (sample.reason[SENSOR_ANEM] == SENSOR_OK)
        printf("Vento: %.2f m/s\r\n", sample.wind_speed);
    else printf("Velocidade do vento: indisponivel\r\n");
    if (sample.reason[SENSOR_VANE] == SENSOR_OK) {
        Wind_Vane_GetLabel(sample.wind_dir_deg, wind_direction_label, sizeof(wind_direction_label));
        printf("Direcao: %s (%u graus)\r\n", wind_direction_label, sample.wind_dir_deg);
    } else printf("Direcao: indisponivel\r\n");
    if (sample.reason[SENSOR_RAIN] == SENSOR_OK) printf("Chuva acumulada: %.2f mm\r\n", sample.rain_mm);
    else printf("Chuva acumulada: indisponivel\r\n");
    uint32_t accepted, rejected, last_pulse, rain_rejected, last_tip;
    anemometer_get_diagnostics(&accepted, &rejected, &last_pulse);
    pluviometer_get_diagnostics(&rain_rejected, &last_tip);
    printf("[PULSOS] Vento janela: aceitos=%lu rejeitados=%lu ultimo_ms=%lu | Chuva total: aceitos=%lu rejeitados=%lu ultimo_ms=%lu\r\n",
        (unsigned long)accepted, (unsigned long)rejected, (unsigned long)last_pulse,
        (unsigned long)last_dados_chuva.tip_count, (unsigned long)rain_rejected, (unsigned long)last_tip);
    printf("SD: %s (erro=%d)\r\n", Storage_IsReady() ? (sd_result == FR_OK ? "OK" : "ERRO") : "INDISPONIVEL",
        (int)(Storage_IsReady() ? sd_result : Storage_GetLastError()));
    printf("==========================================\r\n");
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_I2C1_Init();
  MX_TIM10_Init();
  MX_USART2_UART_Init();
  MX_TIM6_Init();
  MX_ADC1_Init();
  MX_SPI1_Init();
  MX_FATFS_Init();
  MX_SPI2_Init();
  /* USER CODE BEGIN 2 */
  /*
  * MUDANÇAS em relação à versão anterior:
   *  1. SD_Deselect() inicializa apenas o CS do SD — correto.
   *  2. LORA_NSS é posto em HIGH explicitamente antes de qualquer
   *     chamada LoRa (boa prática, evita NSS flutuante no boot).
   *  3. Ordem de chamada: MX_SPI2_Init() já foi chamado antes
   *     (pelo CubeMX em main), então hspi2 está pronto aqui.
   *  4. Diagnóstico estendido: se 0xFF → imprime dica de hardware.
   * ============================================================ */

  /* ── Estado inicial dos barramentos ──────────────────────────────────────
   * SPI1 (SD)  : CS = HIGH (SD_Deselect)
   * SPI2 (LoRa): NSS = HIGH (escrita direta no pino)
   * Garante que nenhum periférico está selecionado durante o boot.
   */
  /* Estado inicial dos barramentos */
  SD_Deselect();
  HAL_GPIO_WritePin(LORA_NSS_GPIO_Port, LORA_NSS_Pin, GPIO_PIN_SET);

  /* ── Verificação do chip ────────────────────────────────────────────────── */
  uint8_t lora_version = 0x00u;

  SX1276_Reset(); /* Reset somente no boot. Sondagens posteriores nao resetam. */
  if (!SX1276_CheckVersion(&lora_version))
  {
      printf("[LORA] FALHA. RegVersion = 0x%02X\r\n", lora_version);
      if (lora_version == 0xFFu)
          printf("[LORA] DICA: 0xFF pode indicar MISO em pull-up ou SPI aberto.\r\n");
  }
  else
  {
      printf("[LORA] SX1276 OK. RegVersion = 0x%02X\r\n", lora_version);

      SX1276_Config_t lora_cfg = SX1276_DEFAULT_CONFIG;
      bool lora_init_ok = SX1276_Init(&lora_cfg);

      if (lora_init_ok)
          printf("[LORA] Init OK. Pronto para TX.\r\n");
      else
          printf("[LORA] Init FALHOU: %s. Reiniciar apos corrigir a causa.\r\n", SX1276_LastError());
  }

  /* ── Sensores ───────────────────────────────────────────────────────────── */
  printf("\r\n=== Inicializando sensores ===\r\n");

  HAL_StatusTypeDef dht_timer_status = HAL_TIM_Base_Start(&htim10);
  DHT22_Init(&dht22, DHT22_GPIO_Port, DHT22_Pin, &htim10);
  printf("[DHT22] %s\r\n", dht_timer_status == HAL_OK ? "Configurado; aguardando primeira leitura" : "Falha ao iniciar TIM10");

  BMP280_Config_t bmp_cfg = BMP280_DEFAULT_CONFIG;
  BMP280_Status_t bmp_st  = BMP280_Init(&bmp280, &hi2c1, BMP280_ADDR_SDO_LOW, &bmp_cfg);
  if (bmp_st == BMP280_OK) printf("[BMP280] OK - I2C1 0x76\r\n");
  else printf("[BMP280] FALHA: %s\r\n", BMP280_StatusStr(bmp_st));

  BH1750_Status_t bh_st = BH1750_Init(&bh1750, &hi2c1,
                                        BH1750_ADDR_LOW, BH1750_MODE_ONCE_H_RES);
  if (bh_st == BH1750_OK) printf("[BH1750] OK - I2C1 0x23\r\n");
  else printf("[BH1750] FALHA: %s\r\n", BH1750_StatusStr(bh_st));

  anemometer_init();
  printf("[ANEM] OK - EXTI PC7 + TIM6\r\n");

  pluviometer_init();
  printf("[PLUV] OK - EXTI PA1\r\n");

  App_WindVane_Init();
  printf("[DIAG] Zero pulsos e direcao constante nao comprovam conexao fisica.\r\n");

  (void)Storage_Init();
  if (Storage_IsReady())
      printf("[SD] OK - Cartao montado. Log em /logs/data.csv\r\n");
  else
      printf("[SD] AVISO - Cartao/SPI indisponivel. Sensores e LoRa continuam ativos.\r\n");

  last_dht_status = DHT22_ERR_TIMEOUT;
  last_bmp_status = bmp_st;
  last_bh_status  = bh_st;
  last_dht_ms     = HAL_GetTick();

  printf("Sistema Inicializado. A iniciar leituras...\r\n");
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
      uint32_t now = HAL_GetTick();
      if (!acquisition_pending) Storage_Service(now);
      if (!acquisition_pending && (uint32_t)(now - last_dht_ms) >= 5000u) {
          acquisition_pending = true;
          acquisition_start_ms = now;
          /* Driver retains start errors for service(); no cycle is skipped. */
          (void)anemometer_start_sampling(&htim6);
          last_dht_status = DHT22_Read(&dht22);
          /* Init/configuration failures require a validated initialization.
           * Transient communication errors are retried on the next cycle. */
          if (bmp280.initialized) last_bmp_status = BMP280_Read(&bmp280);
          if (bh1750.initialized) last_bh_status = BH1750_Read(&bh1750);
          vane_status = Wind_Vane_Read(&wind_direction_deg);
      }
      if (acquisition_pending) {
          now = HAL_GetTick(); /* Sensor reads above include bounded waits. */
          HAL_StatusTypeDef wind_status = anemometer_service(&htim6, now);
          if (wind_status != HAL_BUSY) {
              anemometer_calculate(ANEM_SAMPLE_WINDOW_MS, &last_dados_vento);
              pluviometer_read(&last_dados_chuva);
              App_FinishAcquisition(now, wind_status);
              acquisition_pending = false;
              last_dht_ms = HAL_GetTick();
          }
      }

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 180;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Activate the Over-Drive mode
  */
  if (HAL_PWREx_EnableOverDrive() != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_144CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */
  adc_initialized = true;

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 400000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_32;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief SPI2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI2_Init(void)
{

  /* USER CODE BEGIN SPI2_Init 0 */

  /* USER CODE END SPI2_Init 0 */

  /* USER CODE BEGIN SPI2_Init 1 */

  /* USER CODE END SPI2_Init 1 */
  /* SPI2 parameter configuration*/
  hspi2.Instance = SPI2;
  hspi2.Init.Mode = SPI_MODE_MASTER;
  hspi2.Init.Direction = SPI_DIRECTION_2LINES;
  hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi2.Init.NSS = SPI_NSS_SOFT;
  hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_128;
  hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi2.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI2_Init 2 */

  /* USER CODE END SPI2_Init 2 */

}

/**
  * @brief TIM6 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM6_Init(void)
{

  /* USER CODE BEGIN TIM6_Init 0 */

  /* USER CODE END TIM6_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM6_Init 1 */

  /* USER CODE END TIM6_Init 1 */
  htim6.Instance = TIM6;
  htim6.Init.Prescaler = 8999;
  htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim6.Init.Period = 9999;
  htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim6) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim6, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM6_Init 2 */

  /* USER CODE END TIM6_Init 2 */

}

/**
  * @brief TIM10 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM10_Init(void)
{

  /* USER CODE BEGIN TIM10_Init 0 */

  /* USER CODE END TIM10_Init 0 */

  /* USER CODE BEGIN TIM10_Init 1 */

  /* USER CODE END TIM10_Init 1 */
  htim10.Instance = TIM10;
  htim10.Init.Prescaler = 180-1;
  htim10.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim10.Init.Period = 65535;
  htim10.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim10.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim10) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM10_Init 2 */

  /* USER CODE END TIM10_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA2_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA2_Stream0_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 2, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);
  /* DMA2_Stream3_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream3_IRQn, 2, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream3_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, LORA_RST_Pin|SD_CS_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LORA_NSS_GPIO_Port, LORA_NSS_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(DHT22_GPIO_Port, DHT22_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin : PLUV_RAIN_Pin */
  GPIO_InitStruct.Pin = PLUV_RAIN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(PLUV_RAIN_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LORA_RST_Pin SD_CS_Pin */
  GPIO_InitStruct.Pin = LORA_RST_Pin|SD_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : LORA_NSS_Pin */
  GPIO_InitStruct.Pin = LORA_NSS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(LORA_NSS_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : ANEM_SPEED_Pin */
  GPIO_InitStruct.Pin = ANEM_SPEED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(ANEM_SPEED_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : DHT22_Pin */
  GPIO_InitStruct.Pin = DHT22_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(DHT22_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LORA_DIO0_Pin LORA_DIO1_Pin */
  GPIO_InitStruct.Pin = LORA_DIO0_Pin|LORA_DIO1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI1_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ(EXTI1_IRQn);

  HAL_NVIC_SetPriority(EXTI9_5_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);

  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 2, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM6)
  {
	  dbg_anem_tim_hits++;
	  anemometer_stop_sampling(htim);
  }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == ANEM_SPEED_Pin)
  {
	dbg_anem_exti_hits++;
	anemometer_pulse_callback();
  }

  if (GPIO_Pin == PLUV_RAIN_Pin)
  {
    pluviometer_tip_callback();
  }
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
