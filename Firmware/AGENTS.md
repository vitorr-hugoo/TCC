# Orientações para desenvolvimento e validação do firmware

## Escopo e fontes

Este arquivo rege `Firmware/`, incluindo STM32, gateway ESP32 e testes. Leia-o antes de alterar o firmware e procure instruções mais específicas nos diretórios afetados. Na inspeção de 12/09/2026 não havia outro `AGENTS.md` na raiz do repositório ou em seus subdiretórios. Preserve instruções locais que venham a existir; não duplique aqui seus detalhes.

**[padrao-c-embarcado](skills/padrao-c-embarcado/SKILL.md) é a fonte de verdade para estilo, nomenclatura e documentação C.** Consulte-a para criar, modificar ou revisar C e headers, inclusive testes. Não replique suas regras neste arquivo nem reformate código legado, gerado ou externo fora do escopo. Preserve APIs e comportamento não relacionado à tarefa.

As skills encontradas estão em `Firmware/skills/`. Não foi encontrado `.agents/skills/`; não presuma que esse caminho existe nem mova as skills implicitamente. Algumas skills citam caminhos genéricos `scripts/` e um README de pacote: os wrappers deste repositório estão em `tests/`.

Use código e configuração atuais para confirmar implementação. Os [planos](STM32/docs/plans/) registram requisitos e propostas; os [relatórios](STM32/docs/reports/) registram evidências históricas. Não trate proposta como funcionalidade entregue, resultado antigo como execução atual ou divergência entre fonte e relatório como resolvida.

## Objetivo e arquitetura

O STM32 implementa uma estação meteorológica: adquire sensores, valida uma amostra, registra dados/saúde em serial e SD e transmite por SX1276. O ESP32 recebe LoRa, valida o pacote e publica via Wi-Fi/MQTT.

| Local | Responsabilidade confirmada |
| --- | --- |
| `STM32/Core/Src/main.c` | Inicialização HAL/periféricos; superloop de aquisição em duas fases; fechamento da amostra; serial, SD e LoRa; callbacks de timer/EXTI. |
| `STM32/Core/{Inc,Src}/dht22.*`, `bmp280.*`, `bh1750.*`, `wind_vane.*` | Temperatura/umidade, pressão, iluminância/irradiância estimada e direção do vento. |
| `STM32/Core/{Inc,Src}/anemometer.*`, `pluviometer.*` | Contagem de pulsos, debounce, janela de vento e chuva acumulada. |
| `STM32/Core/{Inc,Src}/sensor_health.*`, `station_sample.*` | Validade e motivos por sensor, transições de saúde, limites e conversão para o pacote. |
| `STM32/Core/{Inc,Src}/storage.*`, `sd_card.*`, `sd_spi.*` | CSV e recuperação de armazenamento; comandos de cartão; transporte SPI1/DMA. `FATFS/Target/user_diskio.c` integra FatFs ao driver SD. |
| `STM32/Core/{Inc,Src}/lora_sx1276.*`, `Core/Inc/lora_payload.h` | Transporte SPI2, configuração/diagnóstico/TX local e contrato binário/CRC. |
| `Gateway ESP32/gateway_esp32_lora_V1.2.ino` | Integração Arduino, rádio, rede, MQTT e watchdog. |
| `Gateway ESP32/lora_protocol.h/.cpp`, `lora_receive.h` | Validação, sequência, silêncio/recuperação e adaptador de leitura limitada, também usados em testes host. |
| `tests/lora/`, `tests/sensors/` | C/C++ de produção ligado a HAL/FatFs simulados; runners Python. |

## Alvos, execução e dependências

- O [IOC](STM32/Firmware_V0.8.ioc) identifica `STM32F446RET6`, LQFP64, placa `NUCLEO-F446RE`; `.cproject` e startup confirmam a família/alvo Cortex-M4. Isso identifica a configuração do projeto, não a revisão física do protótipo.
- STM32 **bare-metal**: `main()` usa `while (1)`, `HAL_GetTick()` e interrupções; `Core/Inc/stm32f4xx_hal_conf.h` define `USE_RTOS=0`. Não há kernel FreeRTOS ou configuração de tasks STM32 na árvore. Headers genéricos CMSIS não demonstram uso de RTOS.
- Gateway usa Arduino `setup()`/`loop()` e `esp_task_wdt`, registrando a task corrente. Não transportar a classificação bare-metal do STM32 para o runtime ESP32. Não há criação de tasks próprias no sketch inspecionado; configuração e fontes do kernel do core ESP32 não estão no repositório.
- IOC: CubeMX 6.17.0, pacote STM32Cube FW_F4 V1.28.3, SYSCLK 180 MHz, APB1 45 MHz e APB2 90 MHz. HAL local declara 1.8.5; CMSIS Core 5.6.0 e camada device STM32F4 2.6.11, nos respectivos headers/fontes.
- Interfaces atuais: DHT22 em PA8/TIM10; BMP280 e BH1750 em I2C1 PB6/PB7; biruta ADC1 canal 0/PA0; anemômetro PC7/EXTI7/TIM6; pluviômetro PA1/EXTI1; SD SPI1 PA5/PA6/PA7 com CS PC5; LoRa SPI2 PB13/PB14/PB15 com NSS PB12, RESET PC4 e DIO0/DIO1 PA11/PA12. Confira IOC, `main.h`, MSP e handlers ao mudar pinagem; referências antigas a PB0 não definem o NSS atual.

## Build e ferramentas

O projeto gerenciado por STM32CubeIDE chama-se `Firmware_V0.8` (`STM32/.project` e `.cproject`). Importar `Firmware/STM32` na IDE e compilar **Debug e Release** é o procedimento documentado em `tests/lora/README.md`. Os diretórios de build configurados são `Firmware/STM32/Debug` e `Firmware/STM32/Release`.

Toolchain configurada: MCU ARM GCC, prefixo `arm-none-eabi-`, FPU `fpv4-sp-d16`, ABI hard, defines `USE_HAL_DRIVER`/`STM32F446xx` e `DEBUG` na variante Debug. Release explicita otimização por tamanho. Ambas usam `STM32F446RETX_FLASH.ld`; `-u _printf_float` está explícito em Debug, não em Release. Confira a saída real de cada variante ao validar logs com ponto flutuante.

Os relatórios 001/002 registram STM32CubeIDE 2.1.1 e GNU Tools for STM32 14.3.rel1. O [relatório 001](STM32/docs/reports/001-relatorio-execucao-correcao-pinout.md), seção 7.2, registra `make -j4 all` na recompilação Debug. Esse comando depende do makefile **gerado pela IDE**, do `make` e da toolchain no ambiente. O runner tenta primeiro `make` no `PATH`; no Windows, se necessário, encontra uma instalação em `C:\ST\STM32CubeIDE_*\STM32CubeIDE` ou o diretório indicado por `STM32CUBEIDE_DIR`/`--stm32cubeide`, acrescentando `make` e `arm-none-eabi-*` apenas ao processo de build:

```powershell
# Partindo de Firmware/, somente após gerar Debug e Release pela IDE:
python tests/compilar_firmware.py --raiz . --timeout 60
```

Na instalação atual, `STM32CUBEIDE_DIR` é `C:\ST\STM32CubeIDE_2.1.1\STM32CubeIDE`. Para outra instalação, defina a variável somente na sessão antes da validação:

```powershell
$env:STM32CUBEIDE_DIR = 'C:\caminho\para\STM32CubeIDE'
python tests/compilar_stm32.py --raiz .
```

O wrapper chama `compilar_stm32.py` para as duas variantes e o script Arduino existente por `compilar_gateway.py`; grava logs em `relatorios/build/`, encerra a árvore de build se o prazo expirar e remove a saída Arduino parcial. Sem os makefiles gerados, `make`, Arduino CLI ou bibliotecas configuradas, informa a pendência sem declarar sucesso.

Os makefiles gerados estão presentes em `STM32/Debug` e `STM32/Release`; CMake e `compile_commands.json` não foram encontrados. O relatório 003 menciona build headless com workspace temporário, mas não fornece a linha de comando completa. Em checkout sem esses makefiles, importe o projeto e gere as variantes antes de usar `make`. As versões detectadas nesta sessão foram GNU Make 4.4.1_st_20231030-1220 e GNU Tools for STM32 14.3.rel1.20251027-0700.

O gateway possui `tests/lora/build_gateway.ps1`, com parâmetros `-ArduinoCli`, `-ExtraLibraries`, `-OutputDirectory`, `-Jobs`, `-Warnings` e `-StatusFile`. Prepara cópia temporária do sketch e chama Arduino CLI `compile --fqbn esp32:esp32:esp32 --warnings default`, sem gravar a placa. O runner remove a saída anterior e aceita sucesso somente após o script registrar `StatusFile` depois do retorno zero do CLI. Antes do build, copie `Gateway ESP32/gateway_credentials.example.h` para `gateway_credentials.h` e preencha as credenciais locais; o arquivo real é ignorado pelo Git. O ambiente historicamente validado foi ESP32 Arduino Core 2.0.17, LoRa 0.8.0 e PubSubClient 2.8; SPI, WiFi e watchdog vêm do core. As bibliotecas ESP32 não estão incorporadas na árvore. Não presumir compatibilidade automática com Core 3.x.

### Comandos existentes e estado atual

Os comandos abaixo partem de **`Firmware/`**. São interfaces confirmadas por leitura dos scripts; não representam execuções aprovadas nesta inspeção.

| Comando | Uso e pré-condições |
| --- | --- |
| `python tests/executar_testes_unitarios.py --raiz .` | Testes host de sensores/integração e LoRa/gateway com GCC e G++ nativos. |
| `python tests/executar_testes.py --raiz . -- python tests/lora/run.py --case radio_ff` | Exercita o wrapper automatizado com um cenário determinístico; não substitui a regressão completa. |
| `python tests/lora/run.py --case radio_ff` | Seleção de cenário do driver; aceita `--cc` para o compilador C. |
| `./tests/lora/build_gateway.ps1` | Build Arduino em PowerShell, sem gravar placa. |
| `python tests/verificar_formatacao.py --raiz . --proprios` | Verifica, sem escrita, as 44 fontes próprias selecionadas em `tests/_fontes_proprias.py`. Procura `clang-format` no PATH e, no ambiente Windows atual, usa como fallback o executável do Arduino IDE quando existe. |
| `python tests/compilar_firmware.py --raiz .` | Build STM32 Debug/Release e gateway ESP32; usa prazo padrão de 300 s e produz logs em `relatorios/build/`. |
| `python tests/compilar_stm32.py --raiz .` | Build STM32 Debug/Release com makefiles gerados pelo STM32CubeIDE; produz logs para warnings. |
| `python tests/compilar_gateway.py --raiz .` | Adaptador do script Arduino existente; prepara e compila o gateway sem gravar placa. |
| `python tests/avaliar_warnings.py --raiz .` | Compara os logs atuais com `tests/warnings_baseline.json`; exige build prévio. |
| `python tests/analisar_firmware.py --raiz .` | Chama clang-tidy/cppcheck disponíveis somente com banco de compilação atual. |
| `python tests/validar_firmware.py --raiz . --config validacao.json` | Agregador dos gates configurados; gera `relatorios/validacao.json` por padrão, sem conceder aprovação técnica. |

Os runners e o script Arduino resolvem `Firmware/` a partir de seus próprios diretórios, sem acrescentar outro componente `Firmware`. A suíte de sensores falha se qualquer arquivo de suíte não contiver casos; a de LoRa falha se sua lista de cenários ficar vazia. Os READMEs partem de `Firmware/`.

Os runners compilam em pasta temporária: C11/C++11, `-Wall -Wextra -Werror`, prazo de compilação de 30 s e prazo por cenário de 3 s. A suíte de sensores extrai regiões `PV`, `USER CODE 0` e o loop de `main.c`; mudanças nos delimitadores exigem revisar a extração. A descoberta por expressão regular não contém gate explícito de zero casos; conferir a contagem real antes de aceitar sucesso.

Os wrappers compartilhados declaram Python 3.10+ e biblioteca padrão. Aceitam `--raiz`, `--timeout` e `--relatorio`. Os códigos de saída 125 e 127 identificam, respectivamente, configuração e ferramenta indisponíveis para os relatórios mecânicos. `tests/executar_analise_estatica.py` oferece:

- `--analisador clang-tidy --banco <compile_commands.json existente> <fontes selecionados>`: exige fontes presentes no banco e aplica `--warnings-as-errors=*`.
- `--analisador cppcheck --banco <compile_commands.json existente>`: usa o banco completo e `--error-exitcode=1`.

Esses argumentos são a sintaxe do wrapper, **não comandos prontos**: o banco ainda não foi identificado. Ambos aceitam `--ferramenta` e `--extra`. `.clang-tidy` seleciona checks de análise/bugs, sem correção automática; `.clang-format` oferece verificação mecânica e não substitui `padrao-c-embarcado`. Não há pipeline CI, configuração MISRA, cobertura, sanitizers ou HIL automatizado identificados. Não declarar conformidade MISRA.

## Código próprio, gerado e externo; preservação CubeMX

- Próprio: módulos de sensores, saúde, amostra, armazenamento e rádio listados acima, gateway, testes/fakes e scripts. `STM32/mudanca_SPI/` contém versões/patches alternativos; não consta nas `sourceEntries` de Debug/Release, que incluem `Core`, `FATFS`, `Middlewares` e `Drivers`. Não usar essa pasta como implementação ativa nem incluí-la automaticamente no build.
- Gerado/misto: `main.c`, `main.h`, MSP, interrupções, configuração HAL, `FATFS/App`, esqueleto `FATFS/Target`, startup, linker e arquivos de projeto. Há lógica própria em regiões de usuário, especialmente `main.c` e `user_diskio.c`; não substituir esses arquivos integralmente.
- Externo: `STM32/Drivers/STM32F4xx_HAL_Driver`, `Drivers/CMSIS` e `Middlewares/Third_Party/FatFs`. Preserve licenças e não aplique estilo autoral ao vendor.
- Preserve todos os marcadores `USER CODE BEGIN/END`, conteúdo e ordem de inicialização necessários. O IOC define `ProjectManager.KeepUserCode=true`, mas isso não protege alterações fora dos marcadores. Código próprio novo deve preferir módulos separados ou regiões destinadas ao usuário.
- Ao alterar configuração de periférico, confronte `.ioc`, funções `MX_*`, `main.h`, MSP e vetores/handlers. Regeneração exige revisão do diff, inclusive FATFS, DMA e callbacks; não aceitar remoções de lógica como mudança cosmética. Preserve nome de projeto e caminhos de linker/build salvo necessidade da tarefa.
- **Ponto sensível conhecido:** `MX_ADC1_Init` trata falhas de `HAL_ADC_Init` e `HAL_ADC_ConfigChannel` desabilitando somente a biruta, para manter a estação em execução. Essas chamadas ficam fora de USER CODE e podem ser reintroduzidas como `Error_Handler()` por uma regeneração CubeMX; revise esse trecho após gerar o projeto. Os testes host extraem e exercitam a função, mas não substituem a inicialização no MCU.

## Memória, concorrência e temporização

- Os dois linkers reservam RAM de 128 KiB em `0x20000000` e Flash de 512 KiB em `0x08000000`. `_Min_Heap_Size=0x200` e `_Min_Stack_Size=0x400` são reservas mínimas; não são medição de uso máximo. O linker RAM existe, mas não é o selecionado nas variantes atuais.
- `Core/Startup/startup_stm32f446retx.s` define `Reset_Handler`, inicializa stack, chama `SystemInit`, copia `.data`, zera `.bss`, chama construtores e `main`. Preserve coerência dos símbolos com o linker e da tabela de vetores com os handlers.
- SPI1/SD usa DMA2 streams 0/3 e buffers internos estáticos de 512 bytes alinhados a 4; a conclusão sinaliza `s_dma_done` via callback. SPI2/LoRa é separado e não usa esse DMA. Não reutilizar buffers em transferência nem confundir propriedade dos chip selects.
- ISR de EXTI conta vento/chuva; TIM6 encerra a janela de vento. Os módulos preservam/restauram PRIMASK em seções críticas. Preserve isso; `volatile` não torna operações compostas atômicas. FatFs está com `_FS_REENTRANT=0`: não introduzir acessos concorrentes ao armazenamento sem rever seu contrato.
- Em `main.c`, a espera de 5000 ms é contada após o fechamento do ciclo anterior, com leituras sequenciais. Não prometer período exato de 5 s: aquisição, SD, serial e TX somam latência. TIM10 é configurado para a base de microssegundos do DHT; TIM6 sustenta janela nominal de 1000 ms, com margem de timeout de 1000 ms em `anemometer.h`.
- Limites atuais em `lora_sx1276.c`: SPI 100 ms, TX 3000 ms e mudança de modo 10 ms. BH1750 usa 180 ms no modo de alta resolução. Gateway observa silêncio após 20000 ms, quando seu loop processa o prazo; rede/serial podem atrasar a observação. `__io_putchar` usa UART com `HAL_MAX_DELAY`; não afirmar que todo caminho do firmware tem espera limitada.
- Preservar invalidação imediata, aviso após três falhas consecutivas e recuperação por leitura válida (`sensor_health.c`, plano 003). Cache antigo não é amostra nova. Zero de chuva/vento/luz ou direção constante não comprova defeito físico.
- Preservar o contrato de **18 bytes**, little-endian, CRC-8 `0x07` iniciado em zero, layout/escalas/sentinelas e nó 1 (`lora_payload.h`, `lora_protocol.*`, plano 002). `TxDone` confirma conclusão local, não entrega. Não introduzir ACK, retransmissão, RTOS STM32 ou reset/reinit generalizado como parte implícita dos planos atuais. A proposta de 19 bytes no plano 003 continua separada.
- Chuva transmitida em centésimos de mm tem teto de representação de 655,35 mm; validade/overflow devem ser avaliados antes da conversão, sem zerar o acumulado local para caber no pacote. Zero remoto permanece ambíguo. Não há orçamento global verificado de stack, WCET, jitter ou latência máxima; medir antes de prometer garantias.

## Quando usar as skills locais

Leia o `SKILL.md` correspondente ao trabalho, sem reproduzir aqui seus checklists:

| Skill | Aplicação |
| --- | --- |
| [padrao-c-embarcado](skills/padrao-c-embarcado/SKILL.md) | Toda criação, alteração e revisão de C/headers; fonte única de estilo, nomes e documentação. |
| [stm32-freertos](skills/stm32-freertos/SKILL.md) | Integração STM32/HAL/CMSIS, CubeMX, clocks, periféricos, ISR/DMA e timing. Usar sua orientação bare-metal no STM32 atual; o nome da skill não autoriza introduzir RTOS. |
| [testes-unitarios-embarcados](skills/testes-unitarios-embarcados/SKILL.md) | Casos determinísticos de lógica, estados, protocolos, limites e erros usando os runners/fakes existentes. |
| [analise-estatica-c](skills/analise-estatica-c/SKILL.md) | Warnings, clang-tidy/cppcheck, banco de compilação, interpretação de diagnósticos e sanitizers host quando configurados. |
| [automacao-testes](skills/automacao-testes/SKILL.md) | Manutenção dos wrappers, caminhos, timeouts, logs e eventual automação de bancada com procedimento/target definidos. |
| [code-review-embarcado](skills/code-review-embarcado/SKILL.md) | Revisão técnica de diff/módulos, riscos e relatório por severidade; revisão somente leitura não autoriza correção. |
| [validacao-firmware](skills/validacao-firmware/SKILL.md) | Fechamento da alteração, agregação das evidências e decisão de prontidão conforme requisitos aplicáveis. |

## Validação exigida pelos requisitos

Antes de executar, associe a mudança aos requisitos dos planos 001–003 e registre etapas obrigatórias. O `validacao.exemplo.json` é um template, não a definição final dos gates do produto. Não rebaixe etapa após uma falha. Para alteração apenas documental, revisar fatos, links e diff é aplicável; isso não equivale a validar o binário.

1. Revisar o diff, preservação de USER CODE/configuração e regras da skill C nos trechos alterados. Executar formatação sem escrita nas fontes pertinentes; evitar varredura cosmética de vendor.
2. Para alteração funcional STM32, compilar Debug e Release; para gateway/protocolo, compilar também o gateway real. Registrar versão, variante, diretório, comando, saída e warnings. Plano 002/B01 exige esses builds; teste host não substitui compilação ARM/ESP32. Comparar warnings com referência da mesma configuração: os relatórios registram `-Wmisleading-indentation` em FatFs `ff.c:5032`, sem que isso constitua supressão automática.
3. Executar regressões pertinentes e conferir contagem real. Plano 002 exige rádio/payload/gateway: erros SPI, IRQ antiga, timeout/rollover, tamanho, CRC, nó, sequência e silêncio. Plano 003 exige sensores/saúde/integração: primeira falha, persistência/recuperação, cache, limites, timer/ADC/I2C, SD ausente e continuidade dos demais subsistemas. Registrar os impedimentos dos runners enquanto persistirem.
4. Executar análise estática quando configurada, com defines/includes/ABI e banco correspondentes ao build. `tests/analisar_firmware.py` procura o banco gerado em `STM32/Debug`, `STM32/Release` ou `STM32/`; não gere banco manualmente. Ausência de banco/ferramenta não significa ausência de diagnósticos. Revisão semântica continua necessária; o agregador Python não a executa.
5. Para concluir os requisitos físicos, cumprir a bancada dos [planos 001](STM32/docs/plans/001-correcao-pinout-stm32f446.md), [002](STM32/docs/plans/002-diagnostico-lora-sem-ack.md) e [003](STM32/docs/plans/003-diagnostico-continuo-dos-sensores.md), conforme área afetada: pinagem/sinais, pulsos e debounce, SD/reinserção, enlace e MQTT, silêncio/recuperação, desconexão de sensores, calibração/LUT e pior tempo de aquisição. O plano 001 prevê 100 pacotes e ensaio integrado de 24 h; o plano 002/B02 prevê 20 pacotes conhecidos e B09 exige observar rede indisponível. São critérios distintos, não resultados já obtidos.
6. Antes de integrar a versão, registrar testes essenciais de bancada, builds e testes aplicáveis aprovados, como exige o plano 002. Relatório 005 relata TX local em bancada; não estender essa evidência para recepção/MQTT, estabilidade prolongada ou todos os sensores. Manter distinção entre simulação e ensaio físico.
7. Entregar evidências vinculadas ao diff final e revisão técnica conforme `validacao-firmware`. Informar PASS/FAIL e distinguir NÃO DISPONÍVEL, NÃO CONFIGURADO, NÃO EXECUTADO e NÃO APLICÁVEL com motivo. Não reutilizar binários antigos após build falho. Ausência de evidência obrigatória impede declarar o firmware validado.

## Limitações e definições pendentes

- Confirmar em execução as contagens dos runners após a realocação de `tests/`. Contagens históricas variam com a revisão e não representam resultados atuais.
- Confirmar em bancada o caminho de inicialização do ADC e revisar `MX_ADC1_Init` após cada regeneração CubeMX; o teste host cobre os retornos HAL simulados, não o periférico real.
- Definir CLI headless STM32 reproduzível e geração do banco de compilação pelo build. Confirmar em cada ambiente as versões instaladas, o baseline de warnings e os gates antes de confiar nos resultados.
- Confirmar revisão física de placas/MCUs, identificação do módulo ESP32, setup de gravação/debug, portas seriais, calibração e medições temporais. O FQBN genérico não identifica sozinho a placa física.
- Caracterizar a biruta: LUT atual cobre ADC 0–4095 e pode confundir cabo aberto com direção válida. O relatório 005 registra contagens falsas de chuva ao girar o anemômetro com cabo externo aberto; interferência elétrica é hipótese de diagnóstico, ainda exige medição, não correção automática de debounce.
- Completar recepção/MQTT, falhas controladas, reinserção SD e estabilidade prolongada com evidência da revisão atual. Diagnósticos ocorridos sem SD não têm fila para gravação retroativa, conforme relatório 003.
- Definir margens globais de memória/stack e limites temporais medidos. Tamanhos de binário e stack pontual de relatórios anteriores não provam pior caso atual.
- Decidir separadamente eventual protocolo de 19 bytes/validade remota e ACK. Não tratar essas propostas como requisitos já aprovados nem alterar o contrato legado silenciosamente.
