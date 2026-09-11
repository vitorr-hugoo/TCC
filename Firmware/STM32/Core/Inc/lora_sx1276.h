/**
 ******************************************************************************
 * @file    lora_sx1276.h
 * @brief   Driver SX1276 via SPI2 — diagnostico e TX local verificada, sem ACK
 *
 * Pinagem (main.h / CubeMX):
 *   SPI2  → PB13 SCK | PB14 MISO | PB15 MOSI
 *   PB12   → LORA_NSS   (Output PP, NSS por software)
 *   PC4   → LORA_RST   (Output PP)
 *   PA11  → LORA_DIO0  (EXTI11, rising — TxDone / RxDone)
 *   PA12  → LORA_DIO1  (EXTI12, rising — RxTimeout)
 ******************************************************************************
 */

#ifndef LORA_SX1276_H
#define LORA_SX1276_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "stm32f4xx_hal.h"
#include <stdbool.h>
#include <stdint.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * MAPA DE REGISTRADORES — SX1276 (os usados nesta implementação)
 * Fonte: SX1276 Datasheet Rev. 7, Tabela 85 (p.108)
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Controle do chip */
#define SX1276_REG_FIFO             0x00u  /* FIFO de TX/RX (janela de 1 byte) */
#define SX1276_REG_OP_MODE          0x01u  /* Modo de operação (Sleep/Standby/TX/RX) */
#define SX1276_REG_FR_MSB           0x06u  /* Frequência portadora [23:16] */
#define SX1276_REG_FR_MID           0x07u  /* Frequência portadora [15:8]  */
#define SX1276_REG_FR_LSB           0x08u  /* Frequência portadora [7:0]   */

/* Potência de saída */
#define SX1276_REG_PA_CONFIG        0x09u  /* Configuração do amplificador PA */
#define SX1276_REG_OCP              0x0Bu  /* Over-current protection */
#define SX1276_REG_LNA              0x0Cu  /* Ganho do LNA (recepção) */
#define SX1276_REG_PA_DAC           0x4Du  /* Modo PA_BOOST de alta potência */

/* FIFO e payload */
#define SX1276_REG_FIFO_ADDR_PTR    0x0Du  /* Ponteiro de acesso ao FIFO */
#define SX1276_REG_FIFO_TX_BASE     0x0Eu  /* Base do buffer TX no FIFO */
#define SX1276_REG_FIFO_RX_BASE     0x0Fu  /* Base do buffer RX no FIFO */
#define SX1276_REG_FIFO_RX_CURRENT  0x10u  /* Endereço atual do RX no FIFO */

/* Configuração do modem LoRa */
#define SX1276_REG_MODEM_CONFIG1    0x1Du  /* BW, CR, header mode */
#define SX1276_REG_MODEM_CONFIG2    0x1Eu  /* SF, CRC, RxTimeout MSB */
#define SX1276_REG_MODEM_CONFIG3    0x26u  /* LowDataRateOptimize, AgcAutoOn */
#define SX1276_REG_PREAMBLE_MSB     0x20u  /* Tamanho do preâmbulo [15:8] */
#define SX1276_REG_PREAMBLE_LSB     0x21u  /* Tamanho do preâmbulo [7:0]  */
#define SX1276_REG_PAYLOAD_LENGTH   0x22u  /* Tamanho do payload em bytes */
#define SX1276_REG_SYMB_TIMEOUT_LSB 0x1Fu  /* Timeout RX em símbolos [7:0] */

/* Status e IRQ */
#define SX1276_REG_IRQ_FLAGS        0x12u  /* Flags de interrupção (RxDone, TxDone...) */
#define SX1276_REG_IRQ_FLAGS_MASK   0x11u  /* Máscara de interrupção */
#define SX1276_REG_MODEM_STAT       0x18u  /* Status do modem */
#define SX1276_REG_PKT_SNR          0x19u  /* SNR do último pacote recebido */
#define SX1276_REG_PKT_RSSI         0x1Au  /* RSSI do último pacote recebido */
#define SX1276_REG_VERSION          0x42u  /* Versão do chip (deve ser 0x12) */



/* ═══════════════════════════════════════════════════════════════════════════
 * VALORES DOS CAMPOS — RegOpMode (0x01)
 *
 * bit 7 : LongRangeMode — 1 = LoRa, 0 = FSK/OOK
 * bit 6 : AccessSharedReg (ignorar)
 * bits 2:0 : Mode
 *   000 = Sleep        → único estado onde bit 7 pode ser alterado!
 *   001 = Standby      → estado padrão após init
 *   010 = FSTx         (não usar)
 *   011 = TX           → transmite e volta ao Standby automaticamente
 *   100 = FSRx         (não usar)
 *   101 = RxContinuous → recepção contínua
 *   110 = RxSingle     → recebe um pacote e volta ao Standby
 *   111 = CAD          (não usar agora)
 * ═══════════════════════════════════════════════════════════════════════════ */
#define SX1276_MODE_SLEEP       0x80u  /* LoRa + Sleep    */
#define SX1276_MODE_STANDBY     0x81u  /* LoRa + Standby  */
#define SX1276_MODE_TX          0x83u  /* LoRa + TX       */
#define SX1276_MODE_RX_CONT     0x85u  /* LoRa + RxCont   */
#define SX1276_MODE_RX_SINGLE   0x86u  /* LoRa + RxSingle */

/* ═══════════════════════════════════════════════════════════════════════════
 * VALORES DOS CAMPOS — RegIrqFlags (0x12)
 * Cada bit é limpo escrevendo 1 nele (write-1-to-clear).
 * ═══════════════════════════════════════════════════════════════════════════ */
#define SX1276_IRQ_RX_TIMEOUT   0x80u  /* Timeout na recepção */
#define SX1276_IRQ_RX_DONE      0x40u  /* Pacote recebido com sucesso */
#define SX1276_IRQ_CRC_ERR      0x20u  /* Erro de CRC no pacote recebido */
#define SX1276_IRQ_VALID_HEADER 0x10u  /* Header válido detectado */
#define SX1276_IRQ_TX_DONE      0x08u  /* Transmissão concluída */
#define SX1276_IRQ_CAD_DONE     0x04u  /* CAD concluído */
#define SX1276_IRQ_FHSS_CHANGE  0x02u  /* FHSS (não usado) */
#define SX1276_IRQ_CAD_DETECTED 0x01u  /* CAD detectou atividade */

/* ═══════════════════════════════════════════════════════════════════════════
 * VERSÃO ESPERADA
 * ═══════════════════════════════════════════════════════════════════════════ */
#define SX1276_VERSION_EXPECTED 0x12u

/* ═══════════════════════════════════════════════════════════════════════════
 * ESTRUTURA DE CONFIGURAÇÃO — passada para SX1276_Init()
 *
 * Isso evita que os parâmetros fiquem hard-coded dentro do driver.
 * O main.c define os valores e passa para Init — fácil de ajustar
 * para testes de alcance no TCC2.
 * ═══════════════════════════════════════════════════════════════════════════ */
typedef struct {
    uint32_t frequency_hz;   /* Frequência em Hz. Ex: 915000000 (915 MHz) */
    uint8_t  tx_power_dbm;   /* Potência TX em dBm: 2 a 17 (PA_BOOST)     */
    uint8_t  spreading_factor; /* SF7 a SF12                               */
    uint8_t  bandwidth;      /* 0=7.8k 1=10.4k 2=15.6k 3=20.8k 4=31.25k
                                 5=41.7k 6=62.5k 7=125k 8=250k 9=500k     */
    uint8_t  coding_rate;    /* 1=4/5  2=4/6  3=4/7  4=4/8                */
    uint8_t  preamble_len;   /* Número de símbolos de preâmbulo (min. 6)   */
    bool     crc_enable;     /* true = CRC habilitado no pacote             */
    bool     implicit_header;/* false = header explícito (recomendado)      */
} SX1276_Config_t;

/* Configuração padrão para bancada: SF9, BW125, CR4/5, 915 MHz, 17 dBm */
#define SX1276_DEFAULT_CONFIG {         \
    .frequency_hz    = 915000000UL,     \
    .tx_power_dbm    = 17u,             \
    .spreading_factor = 9u,             \
    .bandwidth       = 7u,              \
    .coding_rate     = 1u,              \
    .preamble_len    = 8u,              \
    .crc_enable      = true,            \
    .implicit_header = false,           \
}

/* ═══════════════════════════════════════════════════════════════════════════
 * PROTÓTIPOS PÚBLICOS
 * ═══════════════════════════════════════════════════════════════════════════ */

/** Pulsa RESET conforme datasheet §7.2.2 */
bool SX1276_Reset(void);

/** false em erro SPI; so altera *value em sucesso. */
bool SX1276_ReadRegister(uint8_t addr, uint8_t *value);

/** Escreve 1 byte em um registrador via SPI2 */
bool SX1276_WriteRegister(uint8_t addr, uint8_t value);

/** Le REG_VERSION sem reset. Retorna true se SPI OK e valor 0x12. */
bool SX1276_CheckVersion(uint8_t *version);

/** Exige init validada e registradores coerentes, sem reset. */
bool SX1276_CheckHealth(void);

/** Motivo da ultima operacao: OK, ARG, SPI, RADIO, INIT, CONFIG, IRQ, TIMEOUT. */
const char *SX1276_LastError(void);

/**
 * @brief  Configura todos os registradores do modem LoRa.
 *         No boot: Reset, CheckVersion e Init. Nao reinicializa automaticamente.
 *
 * @param  cfg  Ponteiro para a struct de configuração.
 *              Use SX1276_DEFAULT_CONFIG para bancada.
 * @retval true  se todos os registradores foram escritos e verificados.
 *         false se algum registrador não bateu (indica problema de SPI
 *               ou rádio em estado inconsistente).
 */
bool SX1276_Init(const SX1276_Config_t *cfg);

/** true = conclusao LOCAL verificada, nunca confirmacao de entrega. */
bool SX1276_Transmit(const uint8_t *data, uint8_t len);

#ifdef __cplusplus
}
#endif

#endif /* LORA_SX1276_H */
