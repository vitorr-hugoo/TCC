# Testes e compilacao do diagnostico LoRa

## Executar os testes

Pre-requisitos encontrados nesta maquina: Python, GCC e G++ nativos (`C:\MinGW\bin`).

```powershell
cd D:\TCC
python tests/lora/run.py
```

Resultado da implementacao: **50/50 passaram**.

- 26 cenarios ligam o driver C real a uma HAL fake: erros SPI, FF, IRQ antiga, FIFO, perda de configuracao, inicializacao incompleta, timeouts e volta do contador.
- 1 programa testa a rotina CRC e o layout usados pelo STM32 com dois vetores independentes.
- 23 cenarios usam as funcoes reais de validacao/silencio do gateway e seu adaptador de recepcao; incluem 144 inversoes individuais de bits e os casos G01–G23.

Para executar apenas um cenario do driver:

```powershell
python tests/lora/run.py --case radio_ff
```

`--cc C:\MinGW\bin\gcc.exe` permite escolher o compilador C. O compilador C++ e `g++` no PATH. O runner retorna erro em falha de compilacao, assercao ou travamento. Executaveis sao criados em diretorio temporario e removidos ao terminar.

## Compilar o gateway real

```powershell
.\tests\lora\build_gateway.ps1
```

O script prepara um sketch Arduino no diretorio temporario `tcc-lora-arduino/gateway_esp32_lora`, copiando o .ino principal e os tres arquivos auxiliares. Isso atende a exigencia do Arduino de combinar nome da pasta e do .ino, sem renomear o codigo ja existente no repositorio. Nao grava a placa.

Ambiente validado: ESP32 Arduino Core **2.0.17**, placa `esp32:esp32:esp32`, biblioteca LoRa **0.8.0** e PubSubClient **2.8**. O watchdog usa a API desse Core 2.x.

PubSubClient nao estava instalada no ambiente local; para esta verificacao, os fontes oficiais da tag v2.8 foram obtidos apenas no diretorio temporario `tcc-lora-arduino/libraries/PubSubClient`. O script usa essa pasta quando disponivel. Em outro computador, instalar PubSubClient 2.8 pela Library Manager ou CLI, ou indicar `-ExtraLibraries` com uma pasta de bibliotecas. LoRa 0.8.0 ja estava instalada. Os fontes de terceiros nao foram adicionados ao repositorio.

O script aceita `-ArduinoCli`, `-ExtraLibraries` e `-OutputDirectory`. Para usar a IDE, abrir o sketch preparado e manter `lora_protocol.h`, `lora_protocol.cpp` e `lora_receive.h` junto do .ino. As edicoes permanentes devem ser feitas nos arquivos do repositorio; a copia temporaria sera sobrescrita na proxima compilacao.

## Compilar STM32

Importar `Firmware/STM32` no STM32CubeIDE 2.1.1 e compilar Debug e Release. O projeto continua se chamando `Firmware_V0.8`; a branch identifica o desenvolvimento v0.9. A inclusao `lora_payload.h` esta em USER CODE e sera preservada pelo CubeMX.

## Limites e referencia inicial

Os primeiros 16 testes, antes das correcoes, tinham 5 sucessos e 11 falhas. Essas falhas foram corrigidas; o runner nao as ignora como falhas esperadas.

A HAL fake simula registradores, IRQ com limpeza por escrita de 1, finalizacao de TX, GPIO e tempo. Nao simula RF, alimentacao ou todo o SX1276. O reset e contabilizado; nao ha simulacao eletrica completa de reset. O vetor do teste TX e opaco; CRC e interpretacao sao verificados nos programas de payload/gateway.

O relogio falso avanca deterministicamente. O limite externo de 3 segundos por processo detecta travamentos do teste, enquanto os testes de tempo validam os prazos simulados do driver. Compilacao nativa usa avisos como erros; a compilacao ARM/ESP32 separada verifica o codigo nas arquiteturas de destino.

Os testes da logica de aceitacao nao executam MQTT real. A publicacao permanece dentro do ramo de pacote aprovado no loop, conferido na revisao e na compilacao. A validacao de rede, RF, sensores/SD em operacao e o tempo real de silencio exigem bancada, conforme o plano 002.
