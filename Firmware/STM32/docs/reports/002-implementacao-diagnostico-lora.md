# Relatorio 002 — Diagnostico LoRa sem ACK

## Resultado

Implementadas, na branch `desenvolvimento/v0.9`, as etapas A–E do plano 002: acessos SPI verificaveis, inicializacao conferida, verificacao de saude a cada tentativa, conclusao local de TX, validacao do pacote no gateway e aviso de silencio/recuperacao.

**50/50 testes automatizados passaram. STM32 Debug/Release e gateway ESP32 compilaram.** Validacao fisica ainda pendente; nenhuma placa foi gravada durante esta execucao.

Nenhum commit novo foi criado e nenhum push foi executado. main e tag v0.8 permanecem preservadas. O diff e os arquivos novos estao disponiveis para revisao local.

## Mudancas por arquivo

| Arquivo | Mudanca |
|---|---|
| `Core/Src/lora_sx1276.c` | SPI com retorno de erro, timeout BSY, verificacao das configuracoes gravadas, sondagem sem reset, rejeicao de IRQ antiga/incoerente, timeout TX, verificacao final e limpeza limitada |
| `Core/Inc/lora_sx1276.h` | Assinaturas de leitura/escrita com sucesso/falha; CheckHealth e LastError; CheckVersion passa a ser somente leitura |
| `Core/Src/main.c` | Reset explicito no boot; cada amostra chama o driver mesmo apos falha anterior; logs de conclusao local e motivo da falha |
| `Core/Inc/lora_payload.h` | Estrutura e CRC existentes extraidos de main, sem alterar o protocolo; verificacoes de 18 bytes e posicao do CRC em compilacao |
| `Firmware/Gateway ESP32/gateway_esp32_lora_V1.2.ino` | Integracao da validacao antes de publicar/atualizar estado; avisos de silencio e recuperacao; rede depois da recepcao e timeouts de conexao |
| `Firmware/Gateway ESP32/lora_protocol.h/.cpp` | Estrutura de pacote, CRC, validacao de tamanho/no e acompanhamento de sequencia/ultimo valido/silencio, independentes de Arduino/MQTT |
| `Firmware/Gateway ESP32/lora_receive.h` | Adaptador usado pelo LoRa real e fake; tamanho original exato, leitura completa e descarte limitado a 255 bytes |
| `tests/lora` | Driver real com HAL fake, vetores fixos, testes de pacote/gateway, runner e script de compilacao Arduino |

O hash de ambiente que o CubeIDE alterou automaticamente em `.settings/language.settings.xml` foi restaurado. Nao houve mudanca de pinagem, sensores, SD, parametros LoRa padrao, tag ou branch principal.

## Comportamento operacional

- `SX1276_ReadRegister(addr, &value)` so escreve o resultado no ponteiro se a operacao e a liberacao do SPI terminarem corretamente.
- RegVersion deve responder `0x12`; HAL_OK sozinho nao comprova que existe um escravo SPI conectado.
- CheckVersion nao reseta mais o modulo; main chama Reset uma vez no boot.
- Init exige escritas e leituras coerentes, incluindo a verificacao final. Falha nessa ultima leitura tambem deixa TX desabilitado.
- Antes do envio, os registradores precisam corresponder a configuracao validada. Perda confirmada de configuracao exige inicializacao explicita; o firmware nao tenta reiniciar o modulo automaticamente.
- Erros SPI transitorios podem ser reavaliados no proximo ciclo. O sensor/SD nao depende de resposta do gateway; as esperas locais sao limitadas.
- TxDone deve estar apagado antes do envio e aparecer depois. Flags de RX/CAD/FHSS nao sao aceitas nessa operacao exclusivamente TX. A verificacao final exige radio acessivel e em standby.
- O log `TX concluida pelo radio` indica conclusao local. Nao significa que o gateway recebeu. Nao existe ACK ou retransmissao.
- Sequencia continua identificando tentativas/amostras. Saltos e repeticoes sao diagnosticos, nao prova de corrupcao. A transicao 65535 para zero e tratada corretamente.
- No gateway, somente tamanho correto, CRC correto e no 1 autorizam atualizar estado valido e publicar. Valores especiais de falha de sensor permanecem aceitos.
- O aviso de silencio usa inicialmente 20000 ms (`LORA_SILENCE_TIMEOUT_MS`). Pacote invalido nao renova esse prazo. Ha um aviso por entrada em silencio e um aviso ao recuperar.
- Pacote valido repetido renova o diagnostico de comunicacao, mas recebe aviso de sequencia: nao comprova amostra nova.
- A fracao de pacotes validos entre os observados nao e rotulada como taxa de entrega pelo ar; nao contabiliza pacotes que nunca chegaram.

## Verificacao automatizada

Comando: `python tests/lora/run.py` a partir da raiz do repositorio.

| Grupo | Quantidade | Resultado |
|---|---:|---|
| Driver C real + HAL fake | 26 | Passou |
| CRC/layout real do STM32 | 1 | Passou |
| Protocolo, silencio e adaptador real do gateway | 23 | Passou |
| Total | 50 | 50/50 |

Cobertura inclui: SPI com erro, leitura FF, versao inesperada, TxDone antigo, erros de cabecalho/FIFO/comprimento, desconexao durante/depois de TX, modo final incorreto, inicializacao incompleta, perda de configuracao, retry apos erro transitorio, BSY preso e contador de tempo atravessando zero.

No gateway, G01–G21 cobrem todos os casos do plano. G22/G23 acrescentam leitura normal, ausencia de pacote, limites do buffer e radio com bytes disponiveis permanentemente. G04 executa 144 inversoes individuais de bits, incluindo os bits do proprio CRC.

Vetor independente: CRC-8 de `123456789` = `0xF4`. O pacote fixo de 18 bytes usa CRC `0x3F`, conferido por divisao polinomial independente com polinomio `0x107`. A construcao da mensagem no STM32 e a interpretacao no gateway devem coincidir com os bytes e campos fixos do teste.

Esses testes nao executam a rede MQTT real. A chamada de publicacao esta restrita ao ramo `PacketStatus::Valid` do loop, revisado e compilado. O comportamento completo com rede e placa faz parte da bancada.

## Compilacoes reais

### STM32

- STM32CubeIDE 2.1.1, compilador ARM GNU 14.3.rel1, projeto Firmware_V0.8, alvo STM32F446RE.
- Debug: compilado; text 76200, data 504, bss 5728 bytes.
- Release: compilado; text 41872, data 136, bss 5720 bytes.
- Compilacao limpa apresenta um aviso ja existente em `Middlewares/Third_Party/FatFs/src/ff.c:5032` (`-Wmisleading-indentation`). Nenhum erro; sem aviso novo no codigo alterado.
- Workspace de verificacao separado em `%TEMP%/tcc-lora-cube-workspace`. Debug e Release permanecem ignorados pelo Git.

### Gateway

- Arduino CLI incluido na Arduino IDE, ESP32 Arduino Core 2.0.17, FQBN `esp32:esp32:esp32`.
- LoRa 0.8.0 e PubSubClient 2.8.
- Compilacao final: 745385 bytes de programa (56%) e 44896 bytes de variaveis globais (13%). Sem erro.
- PubSubClient 2.8 foi obtida em pasta temporaria porque nao estava instalada; nenhuma biblioteca de terceiros foi adicionada ao repositorio.
- Reproducao: `tests/lora/build_gateway.ps1`. O script copia os fontes para um sketch com nome valido para Arduino e nao grava o ESP32.
- O limite TCP usa a sobrecarga de connect com timeout do Core 2.0.17; o MQTT usa socket timeout de 1 segundo e reutiliza a conexao TCP aberta. Esse comportamento foi conferido no [fonte oficial PubSubClient 2.8](https://github.com/knolleary/pubsubclient/blob/v2.8/src/PubSubClient.cpp).

A tentativa de rede ainda pode ocupar temporariamente o loop; nao foi introduzida uma tarefa concorrente. Atrasos reais, inclusive de serial/rede, devem ser medidos em bancada. O aviso e emitido quando o loop observa o prazo vencido, nao por uma interrupcao de tempo real.

## Proximos testes na placa

1. Gravar e comparar mensagens normais nas seriais do STM32 e ESP32.
2. Confirmar sensores e SD durante uma falha LoRa.
3. Reproduzir falha SPI de forma controlada e conferir ausencia de falso sucesso.
4. Interromper o transmissor e observar silencio/recuperacao no gateway.
5. Introduzir uma alteracao de byte depois de calcular CRC e confirmar descarte.
6. Testar sem Wi-Fi/broker e medir o atraso maximo de recepcao/aviso.
7. Testar reset/perda de configuracao do radio: exigir reinicio explicito da estacao para recuperar.

A retirada de GND nao foi repetida nesta execucao. O simulador nao comprova o comportamento eletrico nem o enlace RF. CRC detecta corrupcao, mas nao comprova a exatidao fisica de uma medicao e nao elimina toda possibilidade de colisao.

Somente apos esses testes a v0.9 deve ser considerada validada para integracao na main.
