# Plano 001 — Correção do pinout do STM32F446 e recuperação do firmware

## Metadados

- **Projeto:** Estação meteorológica de baixo custo
- **Firmware:** Transmissor STM32F446 — V0.7
- **Data de criação:** 2026-09-02
- **Status:** Implementado e compilado — validação física de bancada pendente
- **Referência funcional:** Firmware V0.6
- **Motivação:** Adequar o pinout ao roteamento do primeiro protótipo da PCB sem perder as funcionalidades validadas na V0.6.

## 1. Objetivo

Atualizar o firmware V0.7 para o novo mapeamento físico da PCB, remover a dependência inexistente de Card Detect do módulo SD e recuperar o funcionamento integrado dos seguintes subsistemas:

- aquisição dos sensores meteorológicos;
- contagem do pluviômetro por interrupção;
- armazenamento local em cartão SD;
- transmissão LoRa pelo SX1276;
- recepção e publicação dos dados pelo gateway ESP32;
- operação dos sensores e do LoRa mesmo quando o cartão SD estiver ausente ou apresentar falha.

## 2. Mapeamento definitivo

| Sinal | Pino anterior | Pino novo | Configuração esperada |
|---|---|---|---|
| `PLUV_RAIN` | PC8 | PA1 | Entrada com pull-up, EXTI na borda de descida |
| `SD_CS` | PB1 | PC5 | Saída push-pull, estado inicial alto |
| `LORA_RST` | PB5 | PC4 | Saída push-pull, estado inicial alto |
| `LORA_DIO0` | PB4 | PA11 | Entrada, EXTI na borda de subida |
| `LORA_DIO1` | PC3 | PA12 | Entrada, EXTI na borda de subida |
| `CD_SD` | PC4 | Removido | O módulo utilizado não possui Card Detect |
| `DHT22` | PA8 | PA8 | Deve permanecer como GPIO e não pode ser removido na regeneração |
| `B1` | PC13 | Removido | Botão da placa de desenvolvimento não será utilizado na PCB |

Os demais sinais permanecem inalterados:

| Subsistema | Pinos mantidos |
|---|---|
| Biruta | PA0 / ADC1 |
| UART de diagnóstico | PA2 TX, PA3 RX |
| SD / SPI1 | PA5 SCK, PA6 MISO, PA7 MOSI |
| DHT22 | PA8 / TIM10 como base de tempo |
| LoRa NSS | PB12 |
| I²C1 | PB6 SCL, PB7 SDA |
| LoRa / SPI2 | PB13 SCK, PB14 MISO, PB15 MOSI |
| Anemômetro | PC7 / EXTI7 |

## 3. Estado atual e pré-condição

As alterações mais recentes foram feitas na interface do CubeMX, mas o código ainda não foi regenerado. Portanto, antes de editar os drivers, é obrigatório:

1. Salvar o projeto no CubeMX.
2. Gerar novamente o código.
3. Confirmar que `Firmware_V0.7.ioc`, `Core/Inc/main.h`, `Core/Src/main.c` e os arquivos de interrupção refletem o novo pinout.
4. Confirmar que o CubeMX não removeu código localizado nas regiões `USER CODE`.
5. Criar uma cópia de segurança da versão anterior à regeneração, pois este diretório ainda não está versionado com Git.

> **Atualização de execução (2026-09-02):** o código foi regenerado, PA8/DHT22 foi restaurado, B1 e Card Detect foram removidos, e as etapas de implementação deste plano foram aplicadas. O firmware passou por compilação limpa. Os testes que dependem do protótipo permanecem pendentes porque nenhum ST-Link/STM32 estava conectado durante a execução.

### Critérios da pré-condição

O arquivo `main.h` gerado deve possuir:

- `DHT22_Pin` e `DHT22_GPIO_Port` associados a PA8;
- `PLUV_RAIN_Pin` associado a PA1;
- `SD_CS_Pin` associado a PC5;
- `LORA_RST_Pin` associado a PC4;
- `LORA_DIO0_Pin` associado a PA11;
- `LORA_DIO1_Pin` associado a PA12;
- nenhuma definição de `CD_SD`;
- nenhuma definição de `B1`.

## 4. Etapa 1 — Revisar o código gerado pelo CubeMX

Após a regeneração, verificar os seguintes pontos.

### 4.1 Níveis iniciais

- `SD_CS` em PC5 deve iniciar em nível alto.
- `LORA_RST` em PC4 deve iniciar em nível alto.
- `LORA_NSS` em PB12 deve iniciar em nível alto.
- `DHT22` em PA8 deve iniciar em nível alto.

Isso evita selecionar o cartão ou o rádio durante a inicialização.

### 4.2 Interrupções

- PA1 deve utilizar `EXTI1_IRQn` para o pluviômetro.
- PC7 deve continuar utilizando `EXTI9_5_IRQn` para o anemômetro.
- PA11 e PA12 devem compartilhar `EXTI15_10_IRQn`.
- O handler `EXTI15_10_IRQHandler` não deve mais atender `B1_Pin` depois da remoção do botão.

Prioridades propostas:

| Interrupção | Prioridade proposta |
|---|---:|
| TIM6 | 0 |
| EXTI1 — pluviômetro | 1 |
| EXTI9_5 — anemômetro | 1 |
| EXTI15_10 — LoRa | 2 |

As rotinas de interrupção devem permanecer curtas. Debounce, cálculos e impressão serial não devem ser executados diretamente dentro do handler.

## 5. Etapa 2 — Atualizar o driver de baixo nível do SD

Arquivos principais:

- `Core/Inc/sd_spi.h`
- `Core/Src/sd_spi.c`

Alterações:

1. Remover o hardcode de PB1 usado pelo CS antigo.
2. Remover `SD_DETECT_PORT` e `SD_DETECT_PIN`.
3. Remover a declaração e a implementação de `SD_Card_IsPresent()`.
4. Usar diretamente `SD_CS_GPIO_Port` e `SD_CS_Pin`, gerados pelo CubeMX.
5. Atualizar comentários que ainda mencionem PB1, PC4 ou Card Detect.
6. Garantir que todas as saídas de erro deixem PC5/CS em nível alto.

### Decisão de projeto

O arquivo gerado pelo CubeMX deve ser a única fonte de verdade do pinout. O driver não deve repetir números de pinos ou portas que possam ficar desatualizados em uma mudança futura.

## 6. Etapa 3 — Substituir a lógica de Card Detect

Arquivos principais:

- `Core/Inc/sd_card.h`
- `Core/Src/sd_card.c`
- `FATFS/Target/user_diskio.c`

Sem um sinal físico de Card Detect, o firmware não consegue distinguir:

- cartão ausente;
- cartão sem alimentação;
- mau contato;
- falha elétrica no SPI;
- cartão defeituoso.

Essas situações devem ser tratadas genericamente como **cartão não respondeu**.

Alterações:

1. Remover as chamadas a `SD_Card_IsPresent()` da inicialização, leitura e escrita.
2. Considerar o cartão disponível apenas quando responder à sequência de inicialização SPI.
3. Substituir `SD_ERR_NO_CARD` por `SD_ERR_NOT_RESPONDING` ou `SD_ERR_NOT_READY`.
4. Remover mensagens como `Cartao detectado fisicamente`.
5. Produzir diagnósticos baseados na etapa que falhou, por exemplo:
   - timeout no CMD0;
   - resposta inválida ao CMD8;
   - timeout no ACMD41;
   - erro ao ler OCR;
   - falha de leitura ou escrita de bloco.

## 7. Etapa 4 — Corrigir a velocidade de inicialização do cartão

O SPI1 é inicializado atualmente com prescaler `/32`, aproximadamente 2,81 MHz considerando clock de 90 MHz. Essa frequência é alta demais para a entrada do cartão no modo SPI.

Sequência necessária:

1. Manter PC5/CS alto.
2. Configurar SPI1 com prescaler `/256`, aproximadamente 351,6 kHz.
3. Enviar pelo menos 80 clocks com CS alto.
4. Executar CMD0.
5. Executar CMD8.
6. Executar CMD55 + ACMD41 até o cartão ficar pronto ou ocorrer timeout.
7. Executar CMD58 e identificar SDSC/SDHC.
8. Colocar CS alto.
9. Alterar SPI1 para `/8`, aproximadamente 11,25 MHz.

A chamada de `SPI1_SetSpeed(SPI1_SPEED_SLOW)` deve estar ativa antes dos clocks iniciais. A velocidade rápida somente pode ser habilitada depois de uma inicialização bem-sucedida.

## 8. Etapa 5 — Corrigir o estado do Disk I/O

No arquivo `user_diskio.c`:

1. Atualizar a variável `Stat` em todas as transições.
2. Após inicialização bem-sucedida, remover `STA_NOINIT`.
3. Após timeout ou falha de I/O:
   - definir `hsd.initialized = false`;
   - definir `Stat |= STA_NOINIT`;
   - retornar um erro coerente ao FatFs.
4. Retornar `RES_NOTRDY` quando uma operação for solicitada sem inicialização.
5. Retornar `RES_ERROR` quando uma transação iniciada falhar.
6. Validar unidade física, ponteiros e quantidade de setores.
7. Garantir que `CTRL_SYNC` confirme que o cartão terminou operações pendentes.

### Capacidade do cartão

O valor fixo usado em `GET_SECTOR_COUNT` pode ser mantido somente durante a recuperação inicial do protótipo. Antes da versão final, implementar CMD9 e interpretar o CSD para obter a capacidade real.

## 9. Etapa 6 — Tornar a camada Storage tolerante a falhas

Arquivos principais:

- `Core/Inc/storage.h`
- `Core/Src/storage.c`
- `Core/Src/main.c`

Estados propostos:

```c
typedef enum {
    STORAGE_OFFLINE,
    STORAGE_INITIALIZING,
    STORAGE_READY,
    STORAGE_FAULT
} StorageState_t;
```

Comportamento esperado:

1. No boot, fazer uma tentativa de inicialização.
2. Se o cartão não responder, continuar sensores e LoRa normalmente.
3. Se `f_write`, `f_sync`, leitura ou consulta do sistema de arquivos falhar:
   - colocar o armazenamento em `STORAGE_FAULT`;
   - invalidar o estado do driver;
   - manter o CS alto;
   - desmontar o volume da melhor forma possível;
   - não bloquear a aquisição nem a transmissão.
4. Tentar reinicialização periodicamente, inicialmente a cada 10 segundos.
5. Executar a tentativa entre ciclos de aquisição, nunca durante a janela crítica do anemômetro.
6. Após recuperação:
   - montar o FatFs;
   - criar `/logs` caso necessário;
   - reabrir `data.csv` em modo append;
   - voltar a `STORAGE_READY`.

Uma função periódica, por exemplo `Storage_Service(uint32_t now_ms)`, deve concentrar tentativas e transições. O `main.c` não deve acessar diretamente FatFs, `sd_card` ou `sd_spi`.

### Limitação conhecida

Sem Card Detect, a retirada durante uma escrita pode corromper o sistema de arquivos. O firmware pode detectar o erro na transação seguinte e continuar o restante do sistema, mas não pode tornar hot-swap completamente seguro.

## 10. Etapa 7 — Revisar os demais subsistemas afetados

### 10.1 Pluviômetro

- Confirmar que `EXTI1_IRQHandler` chama `HAL_GPIO_EXTI_IRQHandler(PLUV_RAIN_Pin)`.
- Manter `pluviometer_tip_callback()` no callback comum da HAL.
- Atualizar comentários e mensagens de PC8 para PA1.
- Preservar o debounce e o cálculo de chuva acumulada.

### 10.2 LoRa

- Confirmar que o reset usa as macros de PC4, sem hardcode de PB5.
- Confirmar que DIO0 e DIO1 usam PA11 e PA12.
- Remover referências documentais a PB4, PC3 e PB5.
- Manter SPI2, NSS, parâmetros do modem e payload inalterados.
- O TX atual consulta `RegIrqFlags` por polling; as EXTI dos DIOs devem ser mantidas prontas para evolução futura.

### 10.3 DHT22

- Confirmar a restauração de PA8 após a geração do CubeMX.
- Confirmar que o TIM10 permanece configurado como base de microssegundos.
- Validar que o driver ainda consegue alternar PA8 entre saída e entrada.

### 10.4 Gateway ESP32

Nenhuma mudança é necessária no gateway, pois frequência, configuração LoRa, estrutura do payload e CRC8 não mudam com o novo pinout do transmissor.

## 11. Etapa 8 — Limpeza e rastreabilidade

Depois das alterações, procurar referências residuais aos sinais antigos:

```text
PC8
PB1
PB4
PB5
PC3
CD_SD
SD_DETECT
SD_Card_IsPresent
```

Nem toda ocorrência numérica de `GPIO_PIN_1`, `GPIO_PIN_4`, `GPIO_PIN_5` ou `GPIO_PIN_8` será inválida, pois outros periféricos podem usar o mesmo número em portas diferentes. A revisão deve considerar sempre a combinação porta + pino e o contexto.

Também atualizar:

- mensagens UART de inicialização;
- comentários dos headers;
- documentação do pinout;
- histórico deste plano.

## 12. Estratégia de testes

Os testes devem ser executados por etapas para separar erros elétricos, erros de pinout e erros de protocolo.

### 12.1 Compilação

- Compilar sem símbolos de GPIO indefinidos.
- Confirmar ausência de referências a `CD_SD` e `SD_Card_IsPresent()`.
- Tratar warnings relevantes antes de gravar o microcontrolador.

### 12.2 Verificação elétrica

Com multímetro, osciloscópio ou analisador lógico:

- PC5 deve permanecer alto em repouso e baixar durante transações SD.
- PC4 deve permanecer alto e produzir o pulso de reset esperado no SX1276.
- PA1 deve permanecer alto em repouso e receber pulsos baixos do pluviômetro.
- PA11 e PA12 devem receber os sinais DIO do rádio.
- Confirmar alimentação de 3,3 V e terra comum entre STM32, SD e LoRa.

### 12.3 Pluviômetro

1. Aplicar dez pulsos com espaçamento maior que o debounce.
2. Confirmar dez basculadas.
3. Confirmar aproximadamente 2,794 mm para fator de 0,2794 mm por pulso.
4. Confirmar que bounce não produz contagem múltipla.

### 12.4 LoRa

1. Confirmar `RegVersion = 0x12`.
2. Transmitir pelo menos 100 pacotes.
3. No gateway, confirmar:
   - payload de 18 bytes;
   - CRC8 válido;
   - sequência crescente;
   - valores coerentes;
   - ausência de reinicializações do transmissor.

### 12.5 Cartão SD

1. Inicializar com cartão inserido.
2. Montar o volume e criar `/logs/data.csv`.
3. Confirmar que o cabeçalho aparece uma única vez.
4. Gravar amostras suficientes para executar `f_sync()`.
5. Inicializar sem cartão e confirmar que sensores e LoRa continuam operando.
6. Inserir o cartão e confirmar a recuperação periódica.
7. Simular falha de comunicação e confirmar que o firmware não trava.
8. Conferir o CSV em um computador após desligamento controlado.

### 12.6 Integração e estabilidade

Executar o protótipo integrado por algumas horas e, posteriormente, por 24 horas:

- uma aquisição completa por ciclo;
- um pacote LoRa por ciclo quando o rádio estiver operacional;
- uma linha CSV por ciclo quando o SD estiver disponível;
- nenhuma contagem falsa de chuva durante SPI ou LoRa;
- continuidade de sensores e LoRa durante falha do SD;
- recuperação do SD sem reiniciar o STM32;
- ausência de HardFault, travamento ou crescimento indevido de latência.

## 13. Critérios de conclusão

Este plano será considerado concluído quando:

- [x] O código gerado reflete integralmente o pinout definitivo.
- [x] PA8/DHT22 foi restaurado no código gerado; validação elétrica pendente.
- [x] PC13/B1 e `CD_SD` foram removidos.
- [x] O driver SD usa PC5 por meio das macros geradas pelo CubeMX.
- [x] Não existe dependência de Card Detect.
- [x] O SD foi configurado para iniciar abaixo de 400 kHz e operar depois em frequência rápida; medição física pendente.
- [x] O caminho de falha do SD foi isolado da aquisição e do LoRa; ensaio físico pendente.
- [x] A recuperação periódica do armazenamento foi implementada; ensaio de reinserção pendente.
- [ ] Pluviômetro validado fisicamente em PA1.
- [ ] Reset e DIOs do LoRa validados fisicamente no novo pinout.
- [ ] Gateway recebendo payloads do protótipo remapeado.
- [ ] Teste integrado prolongado for concluído sem travamento.
- [x] Comentários, mensagens e documentação ativa não referenciam o pinout antigo.

## 14. Ordem de execução resumida

1. Salvar e regenerar o projeto no CubeMX.
2. Conferir PA8/DHT22 e remoção de B1/CD.
3. Compilar uma vez para identificar inconsistências geradas.
4. Corrigir a associação de PC5 no driver SD.
5. Remover a lógica de Card Detect.
6. Corrigir as velocidades de inicialização e operação do SPI1.
7. Corrigir os estados de `user_diskio`.
8. Implementar a máquina de estados e a recuperação do armazenamento.
9. Atualizar comentários e mensagens.
10. Testar individualmente pluviômetro, LoRa e SD.
11. Executar os testes de integração e estabilidade.
12. Implementar leitura de CSD/CMD9 antes da versão final.

## 15. Registro de decisões

| Data | Decisão |
|---|---|
| 2026-09-02 | PC5 definido como novo `SD_CS`. |
| 2026-09-02 | `CD_SD` removido porque o módulo utilizado não possui Card Detect. |
| 2026-09-02 | PC4 reservado para `LORA_RST`. |
| 2026-09-02 | PA11 e PA12 definidos como DIO0 e DIO1. |
| 2026-09-02 | PA1 definido como entrada do pluviômetro. |
| 2026-09-02 | B1/PC13 removido por não fazer parte da PCB. |
| 2026-09-02 | V0.6 mantida como referência funcional para comparação. |

## 16. Histórico do documento

| Versão | Data | Alteração |
|---|---|---|
| 1.0 | 2026-09-02 | Criação do plano inicial de correção de pinout e recuperação funcional. |
| 1.1 | 2026-09-02 | Implementação concluída em software, compilação limpa realizada e pendências físicas registradas. |
