# Resumo geral do desenvolvimento da estação meteorológica

## 1. Objetivo do projeto

O projeto consiste em uma estação meteorológica baseada em STM32F446, responsável por adquirir dados dos sensores, registrar as medições em cartão SD e transmiti-las por LoRa para um gateway ESP32. O gateway valida o pacote recebido e o encaminha pela rede.

Este documento consolida as principais decisões, correções e validações realizadas durante o desenvolvimento das versões iniciais até a branch de desenvolvimento atual. Ele deve ser lido junto aos relatórios específicos, que preservam os detalhes técnicos de cada etapa.

## 2. Organização do código e versionamento

O projeto passou a ser versionado no GitHub para substituir o fluxo anterior de criar cópias locais como `firmware v1`, `v2`, `v3` e assim por diante.

Foi adotada a seguinte estrutura:

| Referência | Finalidade | Situação |
|---|---|---|
| `main` | Código estável e validado | Contém a referência `v0.8` |
| tag `v0.8` | Marco imutável da versão inicial do repositório | Aponta para o commit inicial do firmware v0.8 |
| `desenvolvimento/v0.9` | Integração das correções e novas funções | Branch atual de desenvolvimento |

A prática definida foi desenvolver e testar na branch `desenvolvimento/v0.9`. Depois da revisão e validação em bancada, a versão pode ser integrada à `main` e receber uma nova tag. Assim, a referência validada permanece preservada caso uma modificação nova introduza comportamento inesperado.

O commit que reúne a principal evolução descrita neste documento é:

```text
3241537 Implementa diagnostico dos sensores e valida TX LoRa com NSS em PB12
```

Ele está presente tanto localmente quanto no repositório remoto da branch `desenvolvimento/v0.9`.

## 3. Correção de pinagem e recuperação funcional

Antes da etapa de diagnóstico, o firmware foi adaptado ao pinout da PCB e da Nucleo. Essa correção foi necessária porque diversos sinais ainda apontavam para um mapeamento anterior.

| Função | Pino utilizado |
|---|---|
| Biruta | PA0 / ADC1 |
| Pluviômetro | PA1 / EXTI1, borda de descida |
| UART de diagnóstico | PA2 e PA3 |
| LoRa RESET | PC4 |
| SD CS | PC5 |
| I²C1 | PB6 e PB7 |
| Anemômetro | PC7 / EXTI7, borda de descida |
| LoRa NSS | PB12 |
| LoRa SPI2 | PB13, PB14 e PB15 |
| LoRa DIO0 e DIO1 | PA11 e PA12 |

Também foram feitas correções na pilha do cartão SD, incluindo inicialização SPI mais conservadora, obtenção da capacidade real do cartão, sincronização de escrita e recuperação após falha. O objetivo foi manter sensores e LoRa operando mesmo quando o SD está ausente ou indisponível.

## 4. Diagnóstico e confiabilidade do LoRa

### 4.1 Problema observado

Em um teste inicial, o GND do módulo LoRa foi removido, mas a serial continuou indicando tentativas sequenciais de envio. Isso mostrou que a mensagem anterior do firmware não distinguia uma tentativa de transmissão de uma operação realmente aceita pelo rádio.

Foi adotado o objetivo de verificar localmente se o STM32 consegue comunicar-se com o SX1276, configurar seus registradores e receber o sinal interno de conclusão de transmissão. Não foi implementado ACK do gateway nesta etapa, pois ele acrescentaria protocolo, tempo de espera e possíveis retransmissões sem ser necessário para a primeira melhoria de confiabilidade.

### 4.2 Implementação no STM32

O driver `lora_sx1276.c` passou a:

- Ler `RegVersion` e exigir o valor `0x12`, que identifica o SX1276 acessível pelo SPI.
- Propagar erros de SPI e limitar a espera pelo flag BSY.
- Escrever e reler os registradores importantes da configuração LoRa.
- Verificar a saúde do rádio antes de cada transmissão, sem realizar reset automático a cada amostra.
- Limpar flags de IRQ antes do envio e exigir que `TxDone` apareça depois que o modo TX for solicitado.
- Impor prazo máximo para a transmissão e retornar o rádio a Standby após falha ou após conclusão.
- Registrar um motivo legível de falha, como `SPI`, `RADIO`, `CONFIG`, `IRQ` ou `TIMEOUT`.

O log `TX concluida pelo radio` significa que o SX1276 local completou o envio e sinalizou `TxDone`. Ele **não confirma** que o gateway recebeu o pacote. Essa diferença é importante: a garantia de entrega exigiria ACK, ainda não implementado.

### 4.3 Correção do NSS e transição de modo

Durante a validação física, foi encontrado um erro de conexão: o NSS do módulo estava no PB12, enquanto referências anteriores ainda mencionavam PB0. A configuração do CubeMX, os comentários, a documentação e os testes foram atualizados para PB12.

Depois disso, o rádio passou a responder corretamente a `RegVersion = 0x12`, mas a inicialização ainda falhava no registrador `RegOpMode`:

```text
esperado: 0x81  -> LoRa em Standby
lido:     0x80  -> LoRa em Sleep
```

Foi identificado que o driver verificava a mudança de modo imediatamente após solicitá-la. A saída de Sleep pode depender da partida do oscilador do rádio. A solução adotada foi consultar `RegOpMode` a cada 1 ms, por no máximo 10 ms, até confirmar Standby.

Esse prazo é um **limite máximo**, não um atraso fixo: se o registrador já estiver correto na primeira leitura, o firmware segue imediatamente. Se permanecer em Sleep após o prazo, a inicialização continua falhando de forma explícita, sem esconder um possível defeito de alimentação, RESET ou oscilador.

A validação em bancada posterior confirmou:

```text
[LORA] Init OK. Pronto para TX.
[LORA] TX concluida pelo radio | seq=0 | len=18 | motivo=OK
```

Portanto, o caminho local de transmissão do STM32 foi validado após a correção do NSS e da confirmação da transição para Standby.

### 4.4 Gateway ESP32

O gateway passou a validar o pacote antes de atualizar seu estado ou publicar dados:

- tamanho exato de 18 bytes;
- CRC correto;
- `node_id` autorizado;
- acompanhamento de sequência, silêncio e recuperação de comunicação.

Pacotes inválidos não renovam o prazo de comunicação válida. O gateway registra ausência de pacotes após 20 s e registra a recuperação quando recebe novamente um pacote válido. Repetições e saltos de sequência são tratados como diagnóstico, não como prova isolada de defeito.

## 5. Diagnóstico contínuo dos sensores

Foi criado um mecanismo comum de saúde para DHT22, BMP280, BH1750, biruta, anemômetro e pluviômetro. Para cada sensor, o firmware mantém:

- validade da amostra atual;
- motivo da última tentativa;
- quantidade de falhas consecutivas, limitada a 255;
- instante da última leitura válida;
- indicação de que já houve, ou não, uma leitura válida desde o boot.

Uma leitura inválida deixa de ser publicada imediatamente. O último valor válido pode permanecer no cache do driver, mas não é apresentado como se fosse uma medição nova. Três falhas consecutivas geram aviso de falha persistente; uma leitura válida posterior gera aviso de recuperação.

As transições são registradas na serial e no arquivo `sensor_health.csv`. Isso evita repetir a mesma mensagem de erro em todos os ciclos.

### 5.1 Verificações por sensor

| Sensor | Verificações implementadas | Limitação reconhecida |
|---|---|---|
| DHT22 | Temporização em microssegundos pelo TIM10, timeout limitado, checksum, faixa de temperatura e umidade, atualização do cache somente após quadro válido | Uma resposta válida não comprova calibração ou integridade física permanente |
| BMP280 | Chip ID, calibração, configuração, estado de conversão, dados brutos, faixas de pressão e temperatura | Pressão constante pode ser uma leitura legítima |
| BH1750 | Comandos e leitura I²C, medição única a cada ciclo, verificação de saturação e de irradiância derivada | Zero lux pode ser escuridão; o sensor não oferece chip ID ou CRC de medição |
| Biruta | Inicialização ADC, timeout, maioria de 5 entre 8 amostras mapeadas pela LUT | A LUT cobre de 0 a 4095; cabo aberto pode parecer uma direção válida |
| Anemômetro | Retorno de início/parada do TIM6, timeout da janela, pulso aceito/rejeitado, proteção contra IRQ atrasada | Zero pulsos pode ser calmaria, rotor parado ou cabo aberto |
| Pluviômetro | Snapshot atômico, debounce, contador cumulativo, pulsos rejeitados e overflow tratado | Zero basculadas pode ser tempo seco ou cabo aberto |

O anemômetro merece destaque: já existia um timeout no código anterior, mas ele descartava o ciclo inteiro. Agora, se a janela de vento falhar, apenas velocidade do vento é marcada como inválida; DHT22, BMP280, BH1750, biruta, SD e LoRa ainda finalizam normalmente.

## 6. Coerência entre serial, SD e LoRa

No fechamento de cada ciclo é formada uma única amostra. A mesma validade é usada para a serial, o SD e a codificação LoRa, evitando que cada saída aplique regras diferentes.

O arquivo principal continua compatível:

```text
/logs/data.csv
timestamp_ms;temp_c;humidity_pct;pressure_hpa;irrad_wm2;wind_speed_ms;wind_dir_deg;rain_mm
```

Valores inválidos na amostra do SD são registrados como `-999`. O novo arquivo abaixo guarda somente transições de saúde:

```text
/logs/sensor_health.csv
timestamp_ms;sensor;evento;valido;motivo;falhas;tem_ultima_valida;ultima_valida_ms;idade_ms
```

O pacote LoRa permanece com 18 bytes para preservar compatibilidade. Antes de converter valores de ponto flutuante, o firmware verifica se são finitos e cabem no campo de transmissão. Isso evita overflow silencioso.

Uma limitação relevante é a chuva acumulada: o campo de 16 bits em centésimos de milímetro representa até 655,35 mm. Acima desse limite, o total continua correto localmente e no SD, mas o campo LoRa recebe o sentinela zero e a serial informa a limitação. A mesma proteção existe para velocidade de vento fora da representação.

Como o pacote legado usa zero para alguns campos inválidos, o gateway não consegue distinguir todos os zeros reais dos zeros usados como sentinela. Uma evolução futura recomendada é aumentar o pacote para 19 bytes e incluir uma máscara de validade. Essa alteração foi deliberadamente adiada para não quebrar a compatibilidade atual.

## 7. Testes automatizados e compilação

Os testes não substituem a bancada, mas verificam o comportamento do firmware diante de falhas simuladas.

| Conjunto | Cobertura principal | Resultado |
|---|---|---:|
| `tests/lora` | SPI, configuração do SX1276, transição de modo, timeout, CRC, pacote e gateway | 54/54 aprovados |
| `tests/sensors` | Drivers reais com HAL simulada, erros, recuperação, limites, timers, ADC e integração com armazenamento | 75/75 aprovados |
| STM32 Debug e Release | Compilação ARM do projeto `Firmware_V0.8` | Concluída sem erros |
| Gateway ESP32 | Compilação Arduino do código do gateway | Concluída sem erros |

Permanece um aviso preexistente em `Middlewares/Third_Party/FatFs/src/ff.c:5032`, relacionado à indentação no FatFs. Não foram encontrados avisos novos nos arquivos modificados pelo projeto.

## 8. Ocorrência durante o teste de anemômetro e pluviômetro

Durante um teste sem o pluviômetro conectado ao sensor, o cabo que segue para a Nucleo permaneceu ligado em PA1 junto ao GND. Ao girar o anemômetro, algumas contagens falsas apareceram no pluviômetro.

Não foi identificado acoplamento lógico no firmware:

| Medição | Entrada | Interrupção |
|---|---|---|
| Anemômetro | PC7 | EXTI7 |
| Pluviômetro | PA1 | EXTI1 |

Os contadores são independentes. A hipótese mais provável é interferência elétrica captada pelo condutor de sinal do pluviômetro que ficou aberto no lado externo. O pull-up interno do STM32 mantém o sinal em nível alto, mas pode não ser suficientemente forte para uma fiação longa, aberta ou próxima a outros condutores.

O GND comum entre os sensores é necessário. Entretanto, retornos longos, conexões ruins ou sinais paralelos por longos trechos podem permitir acoplamento por impedância comum ou por capacitância entre fios.

As medidas recomendadas para validação e melhoria são:

1. Testar com o fio de sinal do pluviômetro removido também do PA1. Se a contagem falsa desaparecer, o trecho aberto do cabo é a evidência principal.
2. Usar um resistor de pull-up externo de 4,7 kΩ entre cada entrada de pulsos e 3,3 V, próximo à Nucleo. Cada sinal deve ter seu próprio resistor.
3. Conduzir sinal e GND de cada sensor como par, preferencialmente trançado, e evitar paralelismo prolongado com sinais de rádio, SPI ou alimentação.
4. Fazer os retornos de GND convergirem para um ponto próximo à Nucleo, evitando que o retorno de um sensor atravesse o caminho do outro.
5. Se necessário após os testes, medir PA1 com osciloscópio enquanto o anemômetro gira e projetar um filtro RC que preserve os pulsos legítimos.

Não é recomendado aumentar o debounce somente para esconder o efeito, pois isso pode mascarar interferência e também descartar eventos reais de chuva.

## 9. Estado atual e próximos passos

O estado atual é de firmware integrado e compilado, com TX LoRa local validado em bancada. O pacote de comunicação continua sem ACK e com 18 bytes. A branch de desenvolvimento contém as modificações consolidadas no commit `3241537`.

As próximas etapas recomendadas são:

1. Validar a recepção no gateway e a publicação MQTT em condições normais e durante interrupção do transmissor.
2. Testar desconexão controlada de cada sensor, confirmando invalidade imediata, aviso no terceiro ciclo e recuperação após restabelecimento.
3. Caracterizar eletricamente a biruta: direções, transições, cabo aberto e valores ADC reais antes de mudar sua LUT.
4. Validar os sensores de pulsos com eventos conhecidos: número de giros do anemômetro e número de basculadas do pluviômetro.
5. Corrigir a suscetibilidade da entrada de chuva a ruído por meio de fiação e pull-up externo antes de alterar filtros de software.
6. Decidir, depois da validação do enlace, se o protocolo deve evoluir para 19 bytes com máscara de validade e se um ACK é necessário para a aplicação final.

## 10. Documentos relacionados

- [Plano 001 — correção de pinout](../plans/001-correcao-pinout-stm32f446.md)
- [Relatório 001 — execução da correção de pinout](001-relatorio-execucao-correcao-pinout.md)
- [Plano 002 — diagnóstico LoRa sem ACK](../plans/002-diagnostico-lora-sem-ack.md)
- [Relatório 002 — implementação do diagnóstico LoRa](002-implementacao-diagnostico-lora.md)
- [Plano 003 — diagnóstico contínuo dos sensores](../plans/003-diagnostico-continuo-dos-sensores.md)
- [Relatório 003 — implementação do diagnóstico dos sensores](003-implementacao-diagnostico-sensores.md)
- [Relatório 004 — confirmação da transição LoRa para Standby](004-lora-transicao-standby.md)
- [Testes LoRa](../../../tests/lora/README.md)
- [Testes dos sensores](../../../tests/sensors/README.md)
