# Plano 002 — Diagnóstico LoRa após o boot, sem ACK

- Branch de trabalho: `desenvolvimento/v0.9`.
- Estado: etapas A–E implementadas; 50 testes automatizados passaram; STM32 Debug/Release e gateway ESP32 compilados; validacao fisica pendente.
- Escopo: STM32 transmissor e gateway ESP32 presentes neste repositório.
- Restrição: manter o pacote de 18 bytes e o envio periódico; não implementar ACK, retransmissão, protocolo novo, RTOS ou recuperação automática nesta etapa.

## 1. Objetivo e significado de sucesso

Eliminar o falso sucesso quando o SX1276 deixa de responder após a inicialização, detectar pacotes inválidos no gateway e sinalizar ausência prolongada de pacotes válidos.

| Evidência | Significado | Limite |
|---|---|---|
| SPI sem erro e registradores coerentes | Comunicação local com o rádio naquele instante | Não prova funcionamento do enlace RF |
| TxDone novo e estado final coerente | Rádio informou conclusão local | Não confirma recebimento |
| Gateway recebe tamanho, nó e CRC válidos | Pacote recebido passou na validação | CRC não comprova exatidão do sensor e tem possibilidade de colisão |
| Sem pacote válido por 20 s | Ausência de comunicação válida observada no gateway | Não identifica sozinho o componente que falhou |

O STM32 nunca deve imprimir “recebido pelo gateway”. A confirmação remota depende de ACK e está fora deste plano.

## 2. Problemas confirmados na revisão

1. `SX1276_Transmit()` aceita o bit TxDone isolado; uma leitura `0xFF` pode gerar falso sucesso.
2. Escritas simples e em rajada ignoram o retorno HAL.
3. A leitura retorna `0x00` em erro, misturando dado e estado da operação.
4. `LoRa_Deselect()` espera SPI ocupado sem limite.
5. `SX1276_Init()` não confere efetivamente as configurações e retorna sucesso em situações de erro.
6. `lora_ok` reflete o boot, não a saúde atual. `SX1276_CheckVersion()` reseta o rádio, portanto não serve como sondagem periódica.
7. O gateway compara o tamanho copiado ao buffer, não necessariamente o tamanho original recebido: um pacote maior pode parecer ter 18 bytes.
8. O gateway atualiza a sequência antes de validar CRC.
9. Não existe aviso de silêncio baseado no último pacote válido.

A leitura `0xFF` é uma hipótese para o teste de GND, reproduzível por simulação. Não foi medida no protótipo e não prova a causa elétrica daquele episódio.

## 3. Ordem de execução

### Etapa A — Tornar os acessos SPI verificáveis

Arquivos: `Core/Src/lora_sx1276.c` e `Core/Inc/lora_sx1276.h`.

- Separar sucesso/erro do valor lido: retorno booleano e byte por ponteiro, ou helper interno equivalente.
- Propagar erros de escrita de registrador, cabeçalho de rajada e conteúdo do FIFO.
- Limitar a espera de BSY. Mesmo em erro, liberar NSS; não entrar em uma segunda espera ilimitada no tratamento da falha.
- Manter inicialmente timeout SPI de 100 ms e prazo de TX de 3000 ms, existentes no driver. Todas as esperas devem terminar; usar subtração de tempo sem sinal para suportar a volta do contador.
- HAL_OK não é confirmação de escravo presente: conferir conteúdo dos registradores também.
- Corrigir a inicialização: ler e comparar frequência, parâmetros do modem/CRC, potência e modo final usando os valores que foram configurados. Não imprimir OK antes das verificações.

Aceite: falhas SPI e divergência de configuração retornam erro; NSS termina alto; nenhuma espera infinita. Atualizar os comentários para descrever a implementação real.

### Etapa B — Validar cada tentativa de transmissão

1. Validar argumentos sem acessar SPI para ponteiro nulo ou tamanho zero.
2. Ler RegVersion sem reset e exigir `0x12`.
3. Verificar que o rádio permanece configurado em LoRa: comparar os campos relevantes de modem/CRC e frequência com a configuração validada no boot. Isso evita considerar saudável um rádio que reiniciou e perdeu a configuração, mas ainda responde `0x12`.
4. Colocar em standby e conferir o modo.
5. Limpar as IRQs e reler para confirmar TxDone apagado. Não aceitar um TxDone antigo.
6. Definir ponteiro e tamanho, conferir tamanho, escrever os bytes no FIFO e verificar todas as operações SPI.
7. Solicitar TX. Aguardar TxDone dentro do prazo; rejeitar falha de leitura e flags incompatíveis como `0xFF`.
8. Ao detectar TxDone, conferir novamente RegVersion e modo final standby, limpar a flag e concluir somente se as operações forem bem-sucedidas.
9. Em falha, tentar standby quando houver comunicação e retornar erro sem bloquear o restante do sistema indefinidamente.

Não exigir observar o estado TX em uma leitura intermediária: o rádio pode terminar antes da leitura. A evidência será TxDone limpo antes e observado depois, com comunicação e estado coerentes.

Manter `SX1276_Transmit()` com retorno booleano é suficiente. Usar logs curtos para distinguir `SPI`, `RADIO`, `CONFIG`, `IRQ` e `TIMEOUT`; não criar uma máquina de estados extensa só para diagnóstico.

Aceite: sucesso somente após uma transmissão local coerente. Checagem periódica não pulsa RESET. CRC existente e comparação no gateway cobrem integridade; leitura de volta de todo o FIFO não é requisito inicial.

### Etapa C — Integrar o diagnóstico ao ciclo do STM32

Arquivo: `Core/Src/main.c`, preservando as regiões USER CODE.

- Fazer a sondagem de saúde no ciclo já existente de aquisição/envio, inclusive após uma falha. Não criar outra tarefa ou timer.
- Não deixar `lora_ok` do boot ser a única autorização para transmitir. Separar “inicialização validada” de “saúde observada agora” de maneira simples.
- Se o rádio falhar, registrar o motivo e prosseguir com sensores e SD. Não disparar Error_Handler por falha LoRa.
- Uma falha transitória pode ser reavaliada no ciclo seguinte. Se houve reinício do rádio ou perda de configuração, não transmitir até nova inicialização validada. Nesta etapa, reiniciar a estação é o procedimento de recuperação; não fazer reset/reinit automático a cada ciclo.
- Se o boot falhou, manter diagnóstico periódico, mas não transmitir apenas porque RegVersion voltou a responder.
- Manter a sequência como identificador de tentativa/amostra, incrementada uma vez por pacote preparado, inclusive quando a tentativa falha. Saltos não significam automaticamente corrupção.
- Usar `TX concluida pelo radio`, `Falha local` e motivo; não usar mensagem que prometa entrega.

Aceite: desconexão após boot deixa de produzir falso sucesso; nenhuma falha de rádio impede indefinidamente as próximas aquisições. A latência temporária do timeout deve ser medida em bancada.

### Etapa D — Corrigir a aceitação do pacote no gateway

Arquivo: `Firmware/Gateway ESP32/gateway_esp32_lora_V1.2.ino`.

- Preservar os 18 bytes, ordem dos campos, escalas, CRC-8 polinômio `0x07`, valor inicial zero e indicadores de falha existentes.
- Adicionar verificação de tamanho da estrutura em compilação nas duas plataformas.
- Conferir o tamanho informado por `LoRa.parsePacket()` antes da cópia. Rejeitar tamanhos diferentes de 18; consumir bytes restantes com limite baseado no comprimento recebido, sem um laço de drenagem infinito.
- Exigir que a leitura produza exatamente os bytes esperados; rejeitar leitura incompleta.
- Validar CRC e `node_id == 1` antes de atualizar sequência, último recebimento válido ou publicar por MQTT.
- Uma sequência fora da ordem esperada produz diagnóstico, não reprovação de CRC. Suportar a transição 65535 → 0 e reinício da estação sem bloquear a recepção.
- Preservar sentinelas como temperatura/vento `-9990` e direção `999`. Não rejeitar esses valores como lixo. Não criar limites físicos arbitrários nesta etapa.

Aceite: pacote inválido não altera estado de recepção válida nem é publicado. Pacote válido é interpretado com as mesmas unidades do STM32. RSSI/SNR são diagnóstico de enlace, não critério de integridade.

### Etapa E — Aviso de silêncio

- Usar `millis()` e prazo configurável de 20000 ms, inicialmente adequado a aproximadamente quatro ciclos nominais de 5 s.
- Contar desde o início da recepção quando ainda não chegou nenhum pacote válido, e desde o último pacote válido nos demais casos.
- Avaliar o prazo mesmo quando não há pacote, antes do retorno antecipado do loop.
- Emitir um aviso na transição para silêncio e um aviso ao voltar a receber. Não imprimir a mesma mensagem em cada passagem pelo loop.
- Pacote inválido não renova o prazo. Pacote válido repetido demonstra recepção, mas não comprova uma medição nova: manter o diagnóstico de sequência separado do temporizador de comunicação.
- Não condicionar o diagnóstico à conexão Wi-Fi/MQTT. Conferir em bancada se a chamada de reconexão bloqueia a observação; limitar esse bloqueio se ele inviabilizar o prazo, sem redesenhar a infraestrutura de rede.

Aceite: aviso no primeiro processamento em que o tempo sem pacote válido alcançar 20 s; recuperação somente com pacote válido. Não prometer precisão de 20 s se o loop estiver bloqueado por outra chamada.

## 4. Estratégia de testes unitários

### 4.1 Testes executáveis entregues agora

Diretório: `tests/lora` na raiz do repositório.

O runner compila **o driver C real** com GCC nativo, ligado a uma HAL fake. Cada cenário roda em processo independente. Não exige placa, Arduino, MQTT, internet ou instalação de framework de testes. Arquivos compilados ficam em diretório temporário fora do repositório.

```powershell
cd D:\TCC
python tests/lora/run.py
```

| Caso executável | Falha/entrada | Resultado exigido |
|---|---|---|
| invalid_arguments | NULL ou comprimento zero | false, nenhum acesso SPI |
| init_ok | Escritas e leituras coerentes | true |
| init_write_error | HAL falha nas escritas | false |
| init_read_error | HAL falha nas leituras | false |
| init_mismatch | Escrita de frequência não tem efeito | false |
| tx_ok | Rádio saudável | true, bytes exatos no FIFO, uma TX, standby, sem reset |
| radio_ff | SPI devolve FF com HAL_OK | false, nenhuma TX iniciada |
| wrong_version | Identificação inesperada | false, nenhuma TX iniciada |
| read_error | Erro HAL na leitura | false |
| write_error | Erro HAL na escrita com IRQ antiga | false |
| fifo_error | Falha ao escrever conteúdo do FIFO | false, nenhuma TX iniciada |
| stale_done | TxDone não apaga | false, nenhuma TX iniciada |
| disconnect_during_tx | Leituras passam a FF ao iniciar TX | false |
| disconnect_after_done | Perda do rádio logo após ler TxDone | false |
| tx_timeout | TxDone nunca chega | false, prazo limitado, tentativa de standby |
| busy_timeout | SPI permanentemente ocupado | false e retorno limitado; teste interrompe processo preso |

Todos os casos que retornam também exigem NSS liberado. O teste de transmissão verifica ausência de reset durante a tentativa.

**Referencia historica antes das correcoes: 5/16 passaram e 11/16 falharam.** Passaram `invalid_arguments`, `init_ok`, `tx_ok`, `read_error` e `tx_timeout`. Os demais expõem as falhas ainda existentes. O runner retorna código 1 enquanto houver falhas, incluindo travamento; não há marcação que esconda falhas esperadas.

Não alterar a expectativa dos testes para fazer o código antigo passar. Ao evoluir o driver, ampliar o comportamento da HAL fake se novos acessos legítimos forem necessários, mantendo a injeção de falhas e a independência das expectativas.

### 4.2 Casos adicionais do driver durante a implementação

| ID | Preparação e estímulo | Asserções |
|---|---|---|
| D17 | Erro apenas no cabeçalho da rajada | Sem TX, false e NSS liberado |
| D18 | Versão correta, rádio em FSK/configuração perdida | Falha CONFIG, sem sucesso; não recuperar automaticamente |
| D19 | Comprimento escrito não corresponde ao solicitado | Falha antes de TX |
| D20 | TxDone chega, modo final permanece TX ou inválido | false |
| D21 | Contador de tempo começa perto de UINT32_MAX e atravessa zero | Mesmo prazo e resultado do timeout normal |
| D22 | Erro ao limpar IRQ ou finalizar após TxDone | false e retorno limitado |
| D23 | Falha transitória seguida de operação saudável | Segunda tentativa pode funcionar; sem estado falso permanente |

D17–D23 foram implementados no runner, junto dos casos de boot sem inicializacao, reset do radio e falha na ultima leitura da inicializacao.

### 4.3 Testes unitarios do pacote e do gateway implementados nas etapas D/E

Extrair apenas a validação do pacote e a decisão de silêncio para funções pequenas, independentes de Serial, Wi-Fi e MQTT. Passar bytes, comprimento e tempo como argumentos. Compilar essas mesmas funções no firmware e nos testes; não manter uma segunda implementação do algoritmo dentro dos testes.

| ID | Entrada/ação | Resultado esperado |
|---|---|---|
| G01 | CRC-8 de bytes ASCII `123456789` | `0xF4`, vetor de referência independente |
| G02 | Mesmo vetor nas rotinas STM32 e gateway | Ambas correspondem ao valor conhecido; não basta comparar uma com a outra |
| G03 | Pacote fixo válido, nó 1, seq 42 e valores conhecidos | Aceito, todos os campos e escalas corretos |
| G04 | Inverter individualmente cada um dos 144 bits do pacote G03, sem recalcular CRC | Todas as 144 variantes rejeitadas |
| G05 | Comprimentos 0, 1, 17, 19 e 255 | Rejeitados antes de decodificação; sem acesso fora do buffer |
| G06 | 19 bytes cujo prefixo de 18 bytes é um pacote válido | Rejeitado pelo tamanho original, mesmo com prefixo válido |
| G07 | Rádio anuncia 18, mas leitura entrega 17 | Rejeitado |
| G08 | Nó diferente de 1 com CRC recalculado e válido | Rejeitado pelo identificador |
| G09 | Pacote inválido depois de seq válida 42 | last_seq, last_valid_ms e contador válido não mudam; zero publicação |
| G10 | Sequência válida 42 → 43 → 45 | Todos aceitos; aviso de salto somente no último |
| G11 | Sequência válida 65535 → 0 | Sem falso salto por overflow |
| G12 | Primeiro pacote, sequência arbitrária | Aceito sem comparação com sequência inexistente |
| G13 | Nó reinicia e volta a seq 0 | Aceito se íntegro; não confundir salto com corrupção |
| G14 | Sentinelas atuais de falha de sensor, CRC válido | Aceito e decodificado como sensor inválido, não como corrupção de rádio |
| G15 | Pacote válido repetido | Estado de comunicação renovado; diagnóstico de sequência não implica nova amostra |
| G16 | Nenhum pacote desde início da recepção; tempo 19999/20000/20001 ms | Sem aviso antes do limite; um aviso ao atingir o limite |
| G17 | Último válido em t=5000; avaliar em 24999 e 25000 | Alerta apenas no segundo instante |
| G18 | Inválidos chegam continuamente durante 20 s | Não adiam o alerta |
| G19 | Várias chamadas durante silêncio | Um único evento de alerta |
| G20 | Pacote inválido durante silêncio, depois válido | Inválido não recupera; válido gera uma recuperação e reinicia prazo |
| G21 | Contador millis atravessa UINT32_MAX | Temporização idêntica à operação sem overflow |

Para G03, congelar no teste um vetor de bytes com CRC calculado e conferido independentemente, e valores esperados de cada campo. Não construir o resultado esperado chamando a mesma função sob teste. G04 verifica corrupção de um bit, não promete detectar toda combinação de múltiplos erros.

G06/G07 também precisam testar o adaptador que recebe o tamanho do rádio: testar somente a função pura não detectaria o erro atual de truncamento. Usar uma interface fake mínima de recepção, sem simular a pilha Arduino inteira.

## 5. Integração e bancada — critérios de aceite reais

| Teste | Procedimento | Aceite |
|---|---|---|
| B01 | Compilar STM32 Debug/Release e gateway com as ferramentas instaladas | Sem erros; registrar versões e avisos pertinentes |
| B02 | Enviar 20 pacotes conhecidos com nó e sequência visíveis nos dois lados | Bytes/campos correspondem; nenhum pacote inválido publicado |
| B03 | Simular falha SPI depois de boot válido, com injeção controlada | STM32 informa falha local; nunca falso TX concluído |
| B04 | Manter TxDone ausente ou BSY preso na simulação | Retorno dentro do limite; laço principal volta a executar |
| B05 | Desligar gateway mantendo transmissor saudável | STM32 pode informar conclusão local, sem afirmar entrega |
| B06 | Interromper transmissor e observar gateway | Aviso de silêncio; sensores/SD do transmissor não dependem da resposta do gateway |
| B07 | Introduzir corrupção após calcular CRC-8 | Gateway descarta e não renova o tempo do último válido |
| B08 | Voltar a enviar pacote válido | Um aviso de recuperação; recepção normal |
| B09 | Wi-Fi/MQTT indisponível | Recepção e diagnóstico local continuam; medir atraso real do aviso |
| B10 | Rádio perde configuração por reset | Falha CONFIG; não transmitir como saudável apenas por RegVersion correto |

As simulações não provam RF, alimentação, aterramento, temporização elétrica ou resposta do hardware ao retirar GND. Para testes físicos de desconexão, montar a condição com a alimentação desligada e registrar a fiação; priorizar a injeção de falha controlada para reproduzir o falso sucesso sem depender de um estado elétrico indefinido.

## 6. Entrega e organização em commits

1. `Adiciona plano e testes de regressao do diagnostico LoRa` — referencia inicial de testes vermelhos documentada.
2. `Verifica acessos SPI e conclusao local de TX` — etapas A/B e testes D17–D23.
3. `Integra diagnostico LoRa ao ciclo de aquisicao` — etapa C.
4. `Valida pacotes e sinaliza silencio no gateway` — etapas D/E e testes G01–G21.
5. `Registra validacao integrada do LoRa` — resultados de compilação e bancada, limitações e evidências.

Antes de integrar à main: todos os testes unitários aplicáveis devem passar, compilação real concluída e testes de bancada essenciais registrados. Não marcar v0.9 validada apenas porque o simulador passou. Não mover a tag v0.8.

## 7. Situacao da implementacao

- Etapas A/B: acessos SPI e inicializacao verificados, timeout BSY e verificacao de TX concluidos.
- Etapa C: cada aquisicao chama TX com sondagem de saude; sensores e SD mantidos; reset explicito somente no boot.
- Etapas D/E: validacao antes de atualizar estado/publicar e aviso de silencio/recuperacao implementados.
- Gateway: validacao e estado em `lora_protocol.h/.cpp`; adaptador de leitura limitado em `lora_receive.h`.
- STM32: pacote/CRC extraidos para `Core/Inc/lora_payload.h` para testar a mesma rotina de producao.
- Testes: 26 cenarios do driver, 1 programa de CRC/layout STM32 e 23 cenarios do gateway (incluindo todas as 144 inversoes de um bit): **50/50 passaram**.
- Compilacao real: STM32 Debug/Release e ESP32 concluida. Detalhes em `docs/reports/002-implementacao-diagnostico-lora.md`.
- Bancada/RF: pendente; nenhum dispositivo foi gravado nesta execucao.
- ACK, retransmissao e reinicializacao automatica: nao implementados.
- Git: alteracoes locais, sem commits novos e sem push. main e tag v0.8 preservadas.
