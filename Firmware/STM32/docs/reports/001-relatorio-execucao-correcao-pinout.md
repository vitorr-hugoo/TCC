# Relatório 001 — Execução da correção de pinout e recuperação funcional

## 1. Identificação

- **Projeto:** Estação meteorológica de baixo custo
- **Firmware:** Transmissor STM32F446 — V0.7
- **Data:** 2026-09-02
- **Plano relacionado:** [`docs/plans/001-correcao-pinout-stm32f446.md`](../plans/001-correcao-pinout-stm32f446.md)
- **Resultado de software:** Implementado e compilado com sucesso
- **Resultado físico:** Aguardando validação de bancada

## 2. Resumo executivo

O firmware V0.7 foi adaptado ao novo pinout definido para facilitar o roteamento da PCB. A geração feita pelo CubeMX foi auditada, os drivers autorais foram desacoplados dos pinos antigos e a pilha do cartão SD foi reformulada para operar sem Card Detect.

Além da troca de GPIO, foram corrigidos problemas preexistentes que poderiam impedir ou tornar instável a inicialização do cartão:

- frequência SPI excessiva durante a entrada no modo SPI;
- capacidade do cartão definida por um número fixo;
- estado do Disk I/O que não era invalidado após falhas;
- ausência de sincronização real em `CTRL_SYNC`;
- armazenamento permanentemente desabilitado depois do primeiro erro;
- mensagens e comentários incompatíveis com o hardware atual.

O firmware passou por uma compilação incremental e por uma compilação limpa completa. Não havia STM32/ST-Link conectado durante a execução, portanto não foram realizadas gravação do microcontrolador, medições elétricas ou testes com sensores, rádio e cartão reais.

## 3. Proteção e rastreabilidade

Antes das alterações autorais, foi criada uma cópia dos arquivos relevantes em:

```text
docs/backups/2026-09-02-pre-functional-recovery/
```

O backup está no mesmo volume `D:` e contém o `.ioc`, os drivers afetados, `main.c`, integração FatFs e o plano original. Nenhum arquivo foi criado ou baixado no volume `C:`. Ferramentas já instaladas no STM32CubeIDE em `C:` foram apenas executadas em modo de leitura/compilação; todas as saídas do build foram gravadas em `D:`.

## 4. Pinout aplicado e verificado

| Função | Configuração anterior | Configuração final | Situação no código gerado |
|---|---|---|---|
| Pluviômetro | PC8 | PA1 / EXTI1 / falling / pull-up | Confirmada |
| SD CS | PB1 | PC5 / saída push-pull / inicial alto | Confirmada |
| LoRa RESET | PB5 | PC4 / saída push-pull / inicial alto | Confirmada |
| LoRa DIO0 | PB4 | PA11 / EXTI11 / rising | Confirmada |
| LoRa DIO1 | PC3 | PA12 / EXTI12 / rising | Confirmada |
| Card Detect | PC4 | Removido | Confirmada |
| DHT22 | PA8 | PA8 / saída inicial alta | Restaurada e confirmada |
| Botão B1 | PC13 | Removido | Confirmada |

Foram preservados SPI1, SPI2, I²C1, ADC1, UART2, TIM6, TIM10 e o pino PC7 do anemômetro.

### Interrupções

O código gerado contém:

- `EXTI1_IRQHandler` atendendo o pluviômetro em PA1;
- `EXTI9_5_IRQHandler` atendendo o anemômetro em PC7;
- `EXTI15_10_IRQHandler` atendendo DIO0 e DIO1 em PA11/PA12;
- nenhuma referência restante ao botão B1.

A prioridade de `EXTI15_10` foi alterada de 0 para 2 no `.ioc` e em `main.c`. Com isso, TIM6 permanece como evento de maior prioridade e os sinais DIO, que não são críticos no TX por polling atual, não preemptam desnecessariamente as interrupções dos sensores.

## 5. Alterações por arquivo

### `Firmware_V0.7.ioc`

**Alterado:**

- prioridade de `EXTI15_10_IRQn` de 0 para 2.

**Motivo:** os DIOs do SX1276 não são usados como caminho crítico pelo transmissor atual. A prioridade inferior reduz interferência temporal sobre as rotinas dos sensores.

### `Core/Inc/main.h`

**Gerado anteriormente pelo CubeMX e auditado:**

- PA1 para `PLUV_RAIN`;
- PC4 para `LORA_RST`;
- PC5 para `SD_CS`;
- PA8 restaurado para `DHT22`;
- PA11/PA12 para DIO0/DIO1;
- ausência de `CD_SD` e `B1`.

Não foi necessário editar manualmente este arquivo.

### `Core/Src/stm32f4xx_it.c`

**Gerado anteriormente pelo CubeMX e auditado:**

- handler próprio para EXTI1;
- handler de EXTI9_5 apenas para o anemômetro;
- handler compartilhado EXTI15_10 apenas para DIO0/DIO1.

Não foi necessário editar manualmente este arquivo.

### `Core/Inc/sd_spi.h`

**Removido:**

- hardcode de PB1 como CS;
- `SD_DETECT_PORT`;
- `SD_DETECT_PIN`;
- declaração de `SD_Card_IsPresent()`;
- documentação do Card Detect.

**Alterado/adicionado:**

- inclusão de `main.h` para consumir o pinout gerado;
- documentação de PC5 como CS;
- correção das frequências SPI documentadas;
- `SPI1_SetSpeed()` passou a retornar `HAL_StatusTypeDef`.

**Motivo:** impedir divergência entre CubeMX e driver, remover uma funcionalidade inexistente no módulo e permitir tratamento de falha durante a troca de velocidade.

### `Core/Src/sd_spi.c`

**Removido:**

- implementação de `SD_Card_IsPresent()`;
- GPIOs codificados diretamente para o CS antigo;
- comentários referentes a PB1 e Card Detect.

**Alterado:**

- `SD_Select()` e `SD_Deselect()` usam `SD_CS_GPIO_Port` e `SD_CS_Pin`;
- validação dos valores aceitos por `SPI1_SetSpeed()`;
- detecção de SPI ocupado antes da reinicialização;
- propagação do retorno de `HAL_SPI_Init()`.

**Motivo:** usar PC5 com segurança e tornar a troca de prescaler verificável.

### `Core/Inc/sd_card.h`

**Removido:**

- `SD_ERR_NO_CARD`, que pressupunha detecção física.

**Adicionado:**

- `SD_ERR_NOT_RESPONDING`;
- declaração de `SD_SPI_Sync()`;
- documentação compatível com o novo comportamento.

**Motivo:** sem Card Detect, ausência física, mau contato e falha de comunicação são indistinguíveis. O erro agora descreve somente o fato observável: o dispositivo não respondeu.

### `Core/Src/sd_card.c`

Este arquivo recebeu a maior revisão funcional.

**Removido:**

- três consultas a `SD_Card_IsPresent()`;
- mensagem `Cartao detectado fisicamente`;
- dependência do pino CD;
- capacidade fictícia implícita na integração.

**Adicionado:**

- CMD9 para leitura do CSD;
- interpretação de CSD v1 para cartões SDSC;
- interpretação de CSD v2 para SDHC/SDXC;
- cálculo real da quantidade de setores de 512 bytes;
- CMD16 para configurar blocos de 512 bytes em cartões não SDHC;
- validação da resposta do CMD58;
- helper central para abortar transações, elevar CS e invalidar o handle;
- validação de ponteiros e limites de setor;
- `SD_SPI_Sync()` com espera pelo fim do estado busy;
- diagnósticos específicos por etapa da inicialização.

**Alterado:**

- SPI1 passa obrigatoriamente para `/256` antes dos clocks iniciais;
- são enviados exatamente 80 clocks com CS alto;
- SPI1 muda para `/8` somente após inicialização e leitura do CSD;
- falhas de comando, token, DMA e busy invalidam o estado do cartão;
- strings de erro deixaram de mencionar slot ou pino CD.

**Motivo:** cumprir a inicialização SPI do cartão, suportar capacidades reais e permitir recuperação coerente após falhas.

Com PCLK2 de 90 MHz:

- `/256` resulta em aproximadamente 351,6 kHz na inicialização;
- `/8` resulta em aproximadamente 11,25 MHz durante operação.

### `FATFS/Target/user_diskio.c`

**Adicionado/alterado:**

- validação de `pdrv`, buffers e quantidade de setores;
- atualização real da variável `Stat`;
- limpeza de `STA_NOINIT` após inicialização bem-sucedida;
- restauração de `STA_NOINIT` depois de falha;
- invalidação de `hsd.initialized` depois de erro;
- retorno de `RES_NOTRDY` para dispositivo não inicializado;
- retorno de `RES_PARERR` para parâmetros inválidos;
- `CTRL_SYNC` passou a chamar `SD_SPI_Sync()`;
- `GET_SECTOR_COUNT` passou a retornar a capacidade lida do CSD;
- diagnóstico textual quando a inicialização falha.

**Removido:**

- valor fixo de 7.741.440 setores;
- retornos genéricos que mantinham o driver marcado como pronto depois de uma falha.

**Motivo:** alinhar o contrato do driver com o FatFs e permitir uma nova inicialização depois de perda de comunicação.

### `Core/Inc/storage.h`

**Adicionado:**

- `StorageState_t` com estados `OFFLINE`, `INITIALIZING`, `READY` e `FAULT`;
- intervalo de recuperação de 10 segundos;
- `Storage_Service()`;
- `Storage_GetState()`;
- `Storage_IsReady()`;
- `Storage_GetLastError()`.

**Motivo:** transformar o armazenamento em subsistema recuperável e manter a aplicação desacoplada do FatFs e do driver físico.

### `Core/Src/storage.c`

**Adicionado:**

- controle explícito de volume, arquivo aberto, DMA inicializado e próximo retry;
- transição centralizada para `STORAGE_FAULT`;
- fechamento/desmontagem de melhor esforço depois de falha;
- CS alto ao entrar em falha ou desinicializar;
- tentativa de recuperação a cada 10 segundos;
- reabertura do CSV em append depois da recuperação;
- verificação da quantidade de bytes ao gravar o cabeçalho;
- propagação e armazenamento do último erro FatFs;
- mensagens de diagnóstico com código de erro.

**Alterado:**

- falhas em `f_getfree`, `f_write` e `f_sync` colocam o subsistema em `FAULT`;
- chamadas só são aceitas quando volume e arquivo estão efetivamente prontos;
- `Storage_Deinit()` trata também estados parciais.

**Motivo:** anteriormente, o primeiro erro apenas fazia `sd_ok = false` e o cartão nunca era tentado novamente.

### `Core/Src/main.c`

**Removido:**

- variável duplicada `sd_ok`;
- lógica que desabilitava o SD permanentemente após `FR_DISK_ERR` ou `FR_NOT_READY`;
- mensagem do pluviômetro ainda indicando PC8.

**Adicionado/alterado:**

- consulta direta a `Storage_IsReady()`;
- chamada de `Storage_Service()` somente entre janelas ativas de aquisição;
- sensores e LoRa continuam mesmo quando o armazenamento falha;
- impressão de estado e último erro do armazenamento;
- mensagem do pluviômetro atualizada para PA1;
- prioridade EXTI15_10 atualizada para 2.

**Motivo:** eliminar duas fontes de verdade para o estado do SD e impedir que a recuperação interfira na janela ativa de medição do anemômetro.

### `Core/Inc/lora_sx1276.h`

**Alterado:** comentários do pinout para PC4, PA11 e PA12.

**Motivo:** impedir que a documentação interna oriente ligações antigas. O protocolo, SPI2, NSS, payload e configuração do modem não foram alterados.

### `Core/Inc/pluviometer.h` e `Core/Src/pluviometer.c`

**Alterado:** referências documentais de PC8 para PA1.

**Motivo:** manter código e documentação coerentes. A lógica de debounce, contagem e conversão para milímetros foi preservada.

## 6. Itens deliberadamente não alterados

- Firmware do gateway ESP32: o payload e a configuração LoRa não mudaram.
- Algoritmos dos sensores: não dependem do pinout alterado além das macros geradas.
- Diretório `mudanca_SPI`: preservado como material histórico e não faz parte do build ativo.
- Código da HAL/CMSIS e FatFs de terceiros: não foi modificado.
- Formato do CSV e payload LoRa: preservados para manter compatibilidade.

## 7. Verificações executadas

### 7.1 Auditoria estática

Todas as verificações abaixo passaram:

- DHT22 em PA8;
- pluviômetro em PA1;
- SD CS em PC5;
- LoRa RESET em PC4;
- B1 e Card Detect ausentes do código ativo;
- nenhuma referência a `SD_Card_IsPresent`, `SD_DETECT` ou `SD_ERR_NO_CARD`;
- driver SD usando macros do CubeMX;
- presença da velocidade lenta e rápida;
- presença de CMD9 e parser CSD;
- presença da máquina de estados e recuperação automática;
- handlers EXTI compatíveis com o novo pinout.

### 7.2 Compilação

Ferramentas utilizadas:

- GNU Tools for STM32 14.3.rel1;
- `make` fornecido pelo STM32CubeIDE 2.1.1.

Foram executados:

1. build incremental;
2. limpeza completa do diretório `Debug`;
3. recompilação integral com `make -j4 all`.

Resultado:

```text
text     data    bss     total    hexadecimal
75280    496     5696    81472    0x13e40
```

Artefato:

```text
Debug/Firmware_V0.7.elf
SHA-256: 57545A362A1CBF99AD9BB919060EED07BEB9730583CD22B13389ACAB0E47F25D
```

Não ocorreram erros ou warnings nos arquivos autorais modificados. Houve um warning preexistente em `Middlewares/Third_Party/FatFs/src/ff.c`, função `f_setlabel`, relacionado a indentação ambígua. Esse arquivo de terceiros não foi alterado.

## 8. Testes não executados e motivo

Não foi detectado STM32, Nucleo ou ST-Link conectado ao computador. Por isso, os seguintes testes permanecem pendentes:

- gravação do ELF no STM32F446;
- leitura UART do boot;
- medição dos níveis de PC4 e PC5;
- geração de pulsos no pluviômetro em PA1;
- leitura de `RegVersion = 0x12` no SX1276 real;
- recepção dos pacotes pelo gateway;
- montagem e gravação em cartão SD real;
- remoção/reinserção e recuperação;
- teste integrado de algumas horas e ensaio de 24 horas.

Não é possível declarar o protótipo fisicamente validado apenas com a compilação. O resultado atual é um firmware coerente, compilável e preparado para os ensaios de bancada.

## 9. Limitações conhecidas

1. Sem Card Detect, a retirada do cartão só é percebida quando uma operação falha.
2. Retirar o cartão durante escrita pode corromper o sistema de arquivos.
3. A tentativa de recuperação é síncrona e ocorre entre janelas de aquisição. Uma tentativa malsucedida pode atrasar o início do ciclo seguinte, embora não interrompa uma janela do anemômetro já iniciada.
4. Amostras produzidas enquanto o SD está indisponível não são armazenadas em RAM para gravação posterior.
5. `GET_BLOCK_SIZE` continua retornando um setor como granularidade conservadora; a geometria real de apagamento não é interpretada.

## 10. Procedimento recomendado para a bancada

1. Conferir continuidade entre o STM32 e PA1, PC4, PC5, PA11 e PA12.
2. Confirmar PC5 e PC4 em nível alto durante repouso.
3. Gravar `Debug/Firmware_V0.7.elf`.
4. Capturar integralmente o log UART do primeiro boot.
5. Testar primeiro sem cartão e confirmar sensores + LoRa.
6. Reiniciar com cartão inserido e confirmar CMD0, CMD8, ACMD41, CMD58 e CMD9.
7. Conferir criação e conteúdo de `/logs/data.csv`.
8. Aplicar dez pulsos ao pluviômetro e confirmar aproximadamente 2,794 mm.
9. Confirmar 100 pacotes consecutivos no gateway com CRC8 válido.
10. Testar falha e recuperação do SD usando um cartão de teste.
11. Executar ensaio integrado prolongado.

## 11. Estado final

| Área | Estado |
|---|---|
| Pinout CubeMX | Concluído |
| Código gerado | Auditado |
| Driver SD em PC5 | Concluído |
| Remoção de Card Detect | Concluído |
| Inicialização SPI lenta/rápida | Concluído |
| CMD9/CSD e capacidade real | Concluído |
| Integração Disk I/O | Concluído |
| Recuperação automática | Concluído |
| Compilação limpa | Aprovada |
| Gravação no STM32 | Pendente — hardware ausente |
| Testes elétricos e funcionais | Pendentes — hardware ausente |
| Validação prolongada | Pendente |

O firmware está pronto para a etapa de gravação e validação física do protótipo.
