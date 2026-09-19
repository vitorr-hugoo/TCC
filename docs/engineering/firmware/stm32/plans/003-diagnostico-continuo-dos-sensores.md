# Plano 003 — Diagnostico continuo dos sensores

- Data: 2026-09-11.
- Estado: etapas locais 1 a 4 implementadas e validadas em software; bancada/calibracao e etapa opcional de 19 bytes pendentes. Ver [relatorio 003](../reports/003-implementacao-diagnostico-sensores.md).
- Base: desenvolvimento/v0.9, incluindo as alteracoes LoRa do plano 002 ainda presentes no workspace.
- Sensores: DHT22, BMP280, BH1750, biruta analogica, anemometro e pluviometro.
- Objetivo: detectar falhas observaveis apos o boot, impedir uso de amostras invalidas e manter os demais subsistemas funcionando.

## 1. Decisao principal: validar a aquisicao sem prometer autodiagnostico fisico

Uma interface responder nao comprova que a medicao esta correta. Um valor constante nao comprova defeito. CRC do LoRa protege o transporte, nao a leitura que o STM32 colocou no pacote.

| Sensor | Evidencia disponivel por software | Limite que deve aparecer no diagnostico |
|---|---|---|
| DHT22 | Resposta temporal, quadro completo, checksum e faixa dos campos | Nao comprova calibracao ou detecta toda falha interna |
| BMP280 | I2C, identificacao, configuracao, estado de conversao, calibracao e dados | Mesmo valor de pressao repetido pode ser legitimo |
| BH1750 | Comandos/leituras I2C e tempo de medicao | Nao oferece identificacao/CRC equivalente ao SX1276; zero pode ser escuridao |
| Biruta | Conversao ADC e compatibilidade com uma tabela calibrada | ADC funcionando nao comprova que cabo e sensor estao conectados |
| Anemometro | Janela de contagem, timer, interrupcoes e pulsos observados | Zero pulsos pode ser calmaria ou cabo aberto |
| Pluviometro | Contador, interrupcoes, debounce e pulsos observados | Zero basculadas pode ser tempo seco ou cabo aberto |

Nos sensores de pulsos, usar a expressao “aquisicao operacional, sem pulsos” em vez de “sensor fisicamente OK”. Uma basculada/giro observado fornece evidencia funcional naquele momento, mas nao certifica o componente indefinidamente.

## 2. Achados anteriores a implementacao

- DHT22 ja verifica timeout e checksum a cada leitura, mas o timeout e contado em iteracoes; depende de compilacao/CPU. Init apenas configura o handle, apesar do log de boot dizer OK.
- BMP280 ja propaga erros I2C e consulta status. Porem, pode retornar OK depois de uma compensacao de pressao que retornou zero, e nao revalida configuracao no ciclo normal. Um BUSY momentaneo nao deve ser chamado de sensor desconectado.
- BH1750 ja propaga erros I2C. O modo continuo atual nao tem leitura de retorno dos comandos de configuracao. Leitura zero nao permite distinguir escuridao de toda possivel falha interna.
- Biruta ja verifica erros ADC, mas a LUT em main cobre todas as contagens de 0 a 4095. Hoje, nao sobra faixa para diagnosticar uma leitura eletricamente incompativel. App_WindVane_Init chama Error_Handler se a inicializacao falha, interrompendo o sistema inteiro.
- Anemometro ignorava retornos de inicio/parada do timer. A fase B dependia de anemometer_is_ready. A leitura completa de main confirmou um timeout ao final do loop, mas ele descartava o ciclo sem gravar/transmitir. A implementacao unificou esse caminho com o fechamento normal da aquisicao.
- Pluviometro marca valid apos copiar/calcular o contador: isso valida a observacao do contador, nao a presenca fisica do reed switch.
- O pacote LoRa atual usa zero como falha em alguns campos. Lux zero e umidade zero, por exemplo, nao podem ser classificados como falha apenas pelo numero recebido.
- Falha em uma leitura pode deixar o ultimo valor bom no handle. O chamador precisa impedir que esse cache seja apresentado como uma amostra atual.

## 3. Escopo inicial recomendado

Implementar diagnostico local, invalidacao imediata de amostras e isolamento de falhas. Reaproveitar drivers, periodo nominal de 5 segundos e arquitetura atual em duas fases. Nao introduzir RTOS, task por sensor, filtros estatisticos de anomalia, reset automatico generalizado ou heuristica “valor nao mudou = defeito”.

A visibilidade remota completa fica em uma etapa posterior deste mesmo plano, pois exige evoluir explicitamente o pacote. O requisito anterior de manter 18 bytes continua valendo para a primeira etapa local.

## 4. Etapa 1 — Impedir que um sensor interrompa a estacao

Prioridade mais alta: main, anemometro e inicializacao da biruta.

1. Fazer o inicio da janela do anemometro informar sucesso/falha. Conferir tambem a parada do timer.
2. Adicionar um prazo para conclusao da janela. Ponto de partida: janela nominal de 1000 ms + margem configuravel, inicialmente 1000 ms; confirmar a margem com o pior tempo das outras leituras em bancada.
3. Verificar ready ou prazo vencido no loop. Em falha, encerrar/inutilizar a janela, marcar somente vento como invalido e finalizar as demais leituras, SD e LoRa.
4. Permitir um novo ciclo apos a falha; ISR atrasada nao deve concluir uma janela cancelada ou contaminar a proxima. Limpar flags pendentes do timer ao cancelar/iniciar e proteger os estados compartilhados.
5. Preservar o estado anterior das interrupcoes em secoes criticas; nao habilitar interrupcoes incondicionalmente ao sair de um trecho que as encontrou desabilitadas.
6. Substituir Error_Handler em falha recuperavel da inicializacao da biruta por registro de erro e bloqueio apenas dessa leitura. Falhas globais de clock/MCU continuam sendo outra categoria.
7. Nenhum driver deve informar medicao valida antes da primeira leitura/janela concluida. Guardar o resultado real de Init; nao inicializar status de sensores ausentes como OK.

Aceite: falha de timer/ADC ou sensor ausente no boot nao interrompe indefinidamente os outros sensores, o SD ou o diagnostico LoRa.

## 5. Etapa 2 — Padronizar um diagnostico pequeno

Um registro por sensor e suficiente, com: validade da amostra atual, motivo da ultima tentativa, instante da ultima leitura valida, contador saturado de falhas consecutivas e flag indicando se ja houve uma leitura valida.

Nao substituir os retornos especificos de cada driver por uma classe generica grande. Traduzir os retornos no ponto de integracao para poucos motivos: NAO_AVALIADO, OK, COMUNICACAO, TIMEOUT, CHECKSUM, CONFIGURACAO, FORA_DA_FAIXA e LIMITE_DE_MEDICAO, usando somente os aplicaveis a cada sensor.

- A primeira tentativa malsucedida ja invalida a amostra: nunca publicar o ultimo valor bom como se fosse novo.
- Proposta de alarme persistente: tres ciclos malsucedidos consecutivos. Isso reduz repeticao de logs, mas nao adia a invalidacao da primeira amostra ruim.
- BUSY antes do fim do prazo significa aquisicao pendente, nao uma falha completa. Contabilizar falha apenas quando terminar a tentativa sem amostra util.
- Emitir logs na mudanca de estado, ao atingir o limite de persistencia e ao recuperar; nao imprimir o mesmo alerta em toda passagem pelo loop.
- Uma leitura valida encerra a sequencia de erros e registra recuperacao. “Recuperacao” indica que a leitura voltou a passar nas verificacoes.
- Idade da ultima leitura valida e tempo desde o ultimo pulso sao informacoes diferentes. Nao renovar idade de amostra copiando cache, e nao declarar ausencia de chuva como amostra vencida.
- Medicao saturada e fora da faixa sao problemas de qualidade, nao prova de cabo desconectado. O motivo deve permanecer explicito.

Exemplos:

```text
[DHT22] Amostra invalida: CHECKSUM
[DHT22] Falha persistente: 3 ciclos sem leitura valida
[DHT22] Leitura valida restabelecida
[ANEM] Janela concluida: 0 pulsos; presenca fisica nao confirmada
[BH1750] Limite de medicao atingido
```

## 6. Etapa 3 — Verificacoes por sensor

### DHT22

- Preservar protocolo e checksum existentes; separar leitura bruta, validacao e atualizacao do handle.
- Usar tempo real em microssegundos para verificar os pulsos. O timer de medicao precisa estar operacional; manter um limite de seguranca para que timer parado tambem nao prenda a leitura.
- Respeitar o intervalo minimo da ficha tecnica e manter o ciclo nominal de 5 s; nao fazer varias tentativas imediatas para esconder um erro.
- Validar campos decodificados contra as faixas especificadas pelo fabricante. Atualizar temperatura e umidade juntas somente apos validar o quadro inteiro.
- Nao rejeitar quadro apenas porque seus campos sao zero; verificar formato, tempos e faixa. Um checksum valido nao demonstra que um timer parado mediu os pulsos corretamente.
- No boot, imprimir interface configurada/aguardando primeira leitura, sem declarar presenca a partir de DHT22_Init.

### BMP280

- Conferir identidade e configuracoes relevantes, distinguindo perda de configuracao de falha transitoria I2C.
- Validar a leitura de calibracao antes de habilitar compensacao, especialmente coeficiente que pode zerar o divisor. Nao transformar falha matematica em pressao 0 com status OK.
- Rejeitar valores brutos de medicao desabilitada e resultados nao finitos/incompativeis com a faixa especificada. Atualizar os campos somente apos validacao completa.
- Tratar conversao em andamento com prazo calculado a partir da configuracao. Pode haver uma nova consulta limitada dentro da janela de aquisicao; BUSY isolado nao e defeito.
- Manter o modo normal atual nesta primeira revisao. Identidade, ctrl_meas/config e estado de conversao sao evidencias de funcionamento; nao exigir que o numero da pressao mude a cada ciclo.

O sensor possui bits de estado e configuracao que podem ser usados nessa verificacao; o codigo deve respeitar a semantica de conversao e leitura dos registros. Referencia: [datasheet oficial BMP280](https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bmp280-ds001.pdf).

### BH1750

- Reaproveitar os retornos I2C e a leitura de dois bytes. Nao inventar leitura de chip ID ou CRC que o dispositivo nao oferece.
- Proposta simples para tornar cada tentativa explicita: usar o modo de medicao unica ja suportado pelo driver, enviando Power On/comando de medicao e aguardando o prazo correspondente antes da leitura.
- O modo H-Resolution ja usa uma espera de 180 ms no driver. Medir o impacto dessa mudanca na janela total; ela nao exige uma tarefa concorrente.
- Zero lux permanece valido em escuridao. Codigo bruto maximo deve ser identificado como limite de medicao, nao automaticamente como erro I2C ou sensor ausente.
- Manter lux e irradiancia estimada coerentes: se lux estiver invalido/saturado, nao apresentar a irradiancia derivada como medicao precisa.
- Sem reset a cada leitura. Power On e comando de medicao fazem parte da aquisicao em modo unico, nao de uma rotina de recuperacao ilimitada.

### Biruta analogica

- Preservar os timeouts e retornos ADC; verificar limpeza/estado nos caminhos de falha.
- Antes de mudar a LUT, medir valores brutos nas direcoes validas, nas transicoes, com cabo aberto e com condicoes controladas de curto.
- Construir janelas validas com margens justificadas por essas medicoes. Reservar faixas invalidas somente quando forem distinguiveis das posicoes reais.
- Nao classificar automaticamente ADC=0 ou 4095 como falha: a montagem atual ainda precisa ser caracterizada.
- Rever o numero minimo de amostras validas exigido pelo filtro. A proposta inicial e maioria do burst (por exemplo, 5 de 8), a confirmar com os dados de bancada.
- Se cabo aberto produzir o mesmo codigo de uma direcao real, documentar a ambiguidade. Detecta-lo com confianca exigira alteracao eletrica; nao compensar inventando limites de software.

### Anemometro

- Implementar verificacoes de timer e prazo da etapa 1.
- Preservar debounce e tratar zero pulsos em janela concluida como velocidade zero valida para a aquisicao.
- Registrar pulsos aceitos/rejeitados para diagnostico; frequencia excessiva so deve gerar limite fisico apos conhecer a especificacao/calibracao do conjunto.
- Nao diagnosticar cabo aberto ou rotor travado apenas pelo tempo sem pulsos. Confirmacao funcional exige giro controlado em bancada.

### Pluviometro

- Preservar contador acumulado e debounce; validar copia/calculo e proteger concorrencia com a ISR.
- Diferenciar contador observado corretamente de reed switch fisicamente confirmado. Zero chuva continua valido.
- Manter a contagem continua, sem zerar o acumulado a cada diagnostico ou erro de outro sensor.
- Registrar instante da ultima basculada como evidencia de evento, sem usa-lo como prazo de falha por ausencia de chuva.
- Confirmar funcionalidade com basculadas controladas; nao adicionar alarme automatico de “sem chuva = defeito”.

## 7. Etapa 4 — Coerencia da amostra, serial e SD

Formar uma unica amostra com valores e validade no fechamento do ciclo, para serial, SD e transmissao consumirem o mesmo resultado. Nao manter regras divergentes em tres trechos de main.

- Em falha, preservar o ultimo valor bom apenas para consulta, acompanhado de idade. A amostra atual usa sua invalidade/sentinela.
- Verificar isfinite, escala e representabilidade antes de converter float para int16/uint16 do pacote; impedir truncamento/estouro silencioso.
- Rever chuva acumulada no payload: uint16 com centesimos so representa ate 655,35 mm. O contador local deve continuar correto; nao trocar para chuva por intervalo nem reiniciar acumulado implicitamente para caber no pacote.
- Na primeira fase local, manter data.csv compativel e registrar transicoes/motivos em arquivo separado, por exemplo sensor_health.csv. Se for necessario mudar colunas, usar arquivo/esquema novo em vez de misturar linhas com o cabecalho antigo.
- Registro de diagnostico no SD nao pode ser condicao para a estacao continuar; os logs seriais permanecem disponiveis sem cartao.
- BMP280 e BH1750 compartilham I2C1: falha simultanea pode indicar barramento/alimentacao comum. Registrar essa possibilidade sem afirmar que ambos quebraram, e sem resetar o barramento a cada leitura.

## 8. Etapa 5 — Visibilidade remota: proposta para discutir

O pacote atual de 18 bytes nao informa a validade de todos os campos sem ambiguidade. Um zero no campo lux, por exemplo, pode representar escuridao ou a substituicao de uma leitura que falhou.

Para monitoramento remoto, recomendo adicionar **um byte de validade antes do CRC**, totalizando 19 bytes, em uma alteracao separada da deteccao local. Os valores e as escalas existentes permanecem; o CRC passa a cobrir tambem a mascara.

| Bit | Significado quando igual a 1 |
|---|---|
| 0 | Temperatura/umidade do DHT22 validas nesta amostra |
| 1 | Pressao BMP280 valida |
| 2 | Iluminancia BH1750 valida, sem limite de medicao |
| 3 | Aquisicao e representacao da velocidade validas |
| 4 | Direcao da biruta valida |
| 5 | Observacao e representacao do acumulado de chuva validas |
| 6–7 | Reservados, zero |

A mascara informa qualidade/utilidade da amostra, **nao presenca fisica garantida**. Ela tambem permite invalidar um valor que nao cabe na representacao do pacote. Motivos detalhados continuam inicialmente na serial/SD; nao precisam ocupar bytes de cada transmissao.

Migracao proposta: atualizar primeiro o gateway para aceitar explicitamente os formatos de 18 e 19 bytes, diferenciados pelo tamanho e validados separadamente; depois atualizar o STM32. Em mensagens antigas, validade nao informada deve permanecer desconhecida. Nunca truncar 19 para 18 bytes.

No JSON MQTT, expor a mascara/validade e usar null para campos explicitamente invalidos. Conferir compatibilidade com o consumidor/banco antes da troca. Se for obrigatorio manter 18 bytes, limitar a primeira entrega ao diagnostico local e documentar a ambiguidade remota; nao inferir defeito de lux/chuva apenas por valor zero.

Esta etapa e uma proposta de evolucao de protocolo, ainda nao uma mudanca aplicada nem requisito para comecar a melhorar os sensores localmente. Sem ACK.

## 9. Testes unitarios planejados

Usar o mesmo estilo do LoRa: codigo real ligado a HAL fake, funcoes pequenas para validacao e tempo simulado. Nao reimplementar o algoritmo em Python apenas para testa-lo contra si mesmo.

| Grupo | Casos obrigatorios |
|---|---|
| Diagnostico comum | Estado inicial nao avaliado; primeira falha invalida imediatamente; tres falhas geram alarme; uma leitura boa recupera; log nao repete; contador satura; tempo atravessa zero |
| DHT22 | Quadro de referencia positivo/negativo; checksum errado; falta de resposta; pulso fora do prazo; timer parado; campo fora da faixa; zeros legitimos; falha nao atualiza ultimo valor valido |
| BMP280 | Identidade errada; NACK/timeout; configuracao perdida; calibracao invalida; divisor zero; dado desabilitado; BUSY seguido de pronto e BUSY ate o prazo; compensacao com vetor independente; leituras iguais validas |
| BH1750 | Erro no comando e na recepcao; espera correta do modo escolhido; zero no escuro; limite bruto maximo; conversao conhecida; dado invalido nao atualiza irradiancia |
| Biruta | Timeout/start/stop ADC; todas as posicoes e limites da LUT calibrada; valores fora das janelas; poucos pontos validos; burst ruidoso; angulo constante valido; cabo aberto somente se distinguivel em bancada |
| Anemometro | Falha de inicio/parada; ISR ausente; cancelamento por prazo; ISR atrasada; janela seguinte apos erro; zero pulsos; pulsos conhecidos; debounce; volta do tick; estado de interrupcoes preservado |
| Pluviometro | Zero basculadas; contagem conhecida; debounce; acumulado preservado; leitura concorrente segura; volta do contador/tick; zero nao dispara alarme |
| Amostra/saidas | NaN/infinito; limites de conversao; cache antigo nao vira amostra nova; mesma validade em serial/SD/LoRa; chuva acima da capacidade do payload nao da volta silenciosa |
| Integracao | Timer do anemometro falha e demais leituras/SD/TX continuam; sensor ausente no boot nao bloqueia; falha I2C compartilhada nao prende o loop; SD indisponivel nao impede diagnostico |
| Protocolo, se aprovado | 18 e 19 bytes; mascara coberta pelo CRC; cada bit de validade; campos invalidos viram null; pacote legado mantem estado desconhecido; frame maior nunca aceito por truncamento |

Os testes deste plano ainda nao foram implementados ou executados. A suite de 50 testes do LoRa deve permanecer passando ao longo da futura implementacao.

## 10. Bancada e sequencia de entrega

1. Primeiro, concluir a validacao fisica do LoRa ja implementado para manter uma referencia clara.
2. Implementar isolamento de falhas de timer/ADC e os respectivos testes.
3. Introduzir o registro minimo de diagnostico e integrar validade ao fechamento do ciclo.
4. Revisar DHT22, BMP280 e BH1750, um por vez, com testes e desconexao/reconexao controladas.
5. Caracterizar eletricamente a biruta antes de fechar suas faixas de erro.
6. Validar anemometro com giros/pulsos conhecidos e pluviometro com basculadas conhecidas, incluindo ausencia legitima de eventos.
7. Medir o pior tempo de aquisicao com falhas e ajustar os prazos sem paralisar os demais subsistemas.
8. Decidir e implementar a evolucao de protocolo separadamente, se a supervisao remota for necessaria.

Durante a bancada, manter condicoes normais de referencia: escuridao, vento parado, ausencia de chuva e direcao fixa **nao devem gerar falsos alarmes**. Registrar diferenca entre erro de leitura, limite de medicao, ausencia de evento e defeito fisico demonstrado.

Reconexao que recupera uma leitura valida permite retomar o estado operacional. Init incompleta, calibracao invalida ou configuracao perdida exigem inicializacao validada; nao implementar reset/reinit ilimitado nesta primeira fase. Recuperacao automatica com intervalo e limite de tentativas pode ser avaliada depois, se os testes mostrarem necessidade.

Nenhum commit ou push faz parte desta entrega de planejamento. As alteracoes funcionais anteriores do LoRa permanecem intactas.
