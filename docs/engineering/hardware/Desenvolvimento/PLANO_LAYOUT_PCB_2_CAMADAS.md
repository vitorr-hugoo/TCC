# Plano de posicionamento e roteamento — PCB da estação meteorológica

**Revisão:** 1.0 · **Data:** 25/09/2026 · **Projeto-fonte:** `hardware/kicad/estacao-meteorologica/`.

> **Atualização de 02/10/2026:** este é o plano inicial. A implementação posterior substituiu o fluxo de potência `+5V → +3V3` por `VIN_MAIN → +3V3`, adotou CR1225 e o socket microSD SOFNG TF-15x15, e definiu a estratégia de aterramento/roteamento. Para o estado vigente, use [DECISOES_IMPLEMENTACAO_PCB_2026-10.md](DECISOES_IMPLEMENTACAO_PCB_2026-10.md). Valores, referências e fluxos antigos deste plano devem ser tratados como históricos quando divergirem do adendo.

Este é o plano de execução do layout da placa principal. Ele parte do esquemático ativo e da BOM atual; não é ainda uma liberação para fabricação. A PCB existente é apenas um rascunho de posicionamento: na inspeção de 25/09/2026 ela não tinha contorno válido e o DRC indicou 283 itens desconectados. Portanto, deve-se sincronizar o esquemático, limpar/posicionar a placa e rotear novamente segundo este documento, sem aproveitar trilhas existentes sem revisão.

## 1. Premissas e arquitetura física

- Placa inicial: **150 mm × 60 mm**, FR-4, 1,6 mm, duas camadas, cobre de 1 oz. É uma meta de envelope, não uma restrição: aumentar a largura para 70 mm é preferível a estreitar loops de potência, reduzir o plano de GND ou invadir a área RF.
- Componentes, serigrafia e montagem preferencialmente em **F.Cu/F.SilkS**. B.Cu é um **plano de GND contínuo**, sem trilhas de sinal; vias e, excepcionalmente, trechos curtos de alimentação só são aceitáveis quando a continuidade do plano for preservada. Conectores PTH podem ser montados pelo lado B por motivo mecânico, pois o barril metalizado é elétrico nas duas faces; isto não obriga rotear seus sinais em B.Cu.
- Usar uma orientação única no desenho: a face dos conectores externos voltada para a borda longa inferior; a extremidade superior reserva RF. Definir esse sentido no desenho mecânico antes de mover footprints.
- Quatro furos M3 (H1–H4) nos cantos, com centros a pelo menos 4 mm das bordas e área de exclusão mecânica de 5 mm ao redor até receber o desenho do gabinete. Se o gabinete usar espaçadores metálicos, manter o anel de cobre dos furos isolado; só aterrar por decisão explícita de EMC/mecânica.
- Distância de cobre à borda: **mínimo 0,50 mm**; distância de corpo de componente à borda: **mínimo 1,0 mm**, exceto conectores projetados para a borda. A capacidade publicada pela JLCPCB é menor, mas esta margem tolera roteamento, fresagem e montagem. [Capacidades da JLCPCB](https://jlcpcb.com/capabilities/Capab).

### Floorplan proposto

| Região física | Conteúdo | Objetivo de posicionamento |
| --- | --- | --- |
| Borda longa inferior, da esquerda para a direita | J60/J61/JP60, J30–J32, J17, J3/J2 | Cabos, cartão e depuração acessíveis sem cruzar a placa. Agrupar conectores de sensores no mesmo lado simplifica o chicote. |
| Canto inferior esquerdo | proteção e bucks da folha `alimentacao` | Confinar corrente pulsante, TVS e calor longe de sensores e RF. |
| Centro | U4, clocks, SW1, SWD, desacoplamentos | Caminhos curtos para todos os blocos e distribuição radial de sinais. |
| Lado esquerdo/centro, próximo de J30–J32 | filtros de pluviômetro, anemômetro e biruta | O ESD fica imediatamente após o conector; sinais filtrados seguem para o MCU sem cruzar potência. |
| Lado direito, próximo de abertura externa | U1, U8, U9 | Sensores ambientais expostos ao ar/luz, afastados de indutores, reguladores e dissipação. |
| Extremidade superior | U12, D70 e J70 | Caminho RF curto, conector U.FL acessível e área livre de cobre/metal conforme a antena escolhida. |

Antes do posicionamento fino, desenhar `Edge.Cuts`, furos, recortes do gabinete, áreas de inserção/ejeção do microSD, altura máxima permitida e keep-outs. Importar, se houver, o DXF mecânico do gabinete. A área do microSD não pode ser comprometida por conector, parafuso ou componente alto.

## 2. Regras de fabricação e classes no KiCad

Os limites absolutos da JLCPCB para placa de duas camadas e 1 oz são 0,10 mm de trilha/espaçamento e 0,15 mm de furo de via; a própria fabricante recomenda evitar perseguir esses mínimos. A classe abaixo é deliberadamente mais conservadora e econômica. [Capacidades de PCB](https://jlcpcb.com/capabilities/Capab).

### 2.1 Configuração global

| Regra KiCad | Valor a configurar | Observação |
| --- | ---: | --- |
| Clearance padrão cobre–cobre | 0,20 mm | Aplicar a sinais e alimentação de baixa tensão. |
| Clearance em potência de entrada / nó SW | 0,30 mm | Para legibilidade, robustez e contenção de ruído; não substitui análise de tensão. |
| Largura mínima de trilha | 0,20 mm | Não usar 0,10 mm como regra normal. |
| Cobre até borda | 0,50 mm | Inclui zonas, trilhas, vias e pads que não sejam conectores de borda. |
| Via padrão | furo 0,30 mm / diâmetro 0,60 mm | Anel de 0,15 mm; usar para sinais e costura de GND. |
| Via de potência | furo 0,40 mm / diâmetro 0,80 mm | Usar várias em paralelo, nunca uma única via como gargalo de um barramento. |
| Anel anular PTH | ≥ 0,25 mm preferível | Conferir nos footprints THT; a recomendação da JLCPCB para 1 oz em duas camadas é ≥ 0,25 mm. |
| Máscara de solda e pasta | valores dos footprints verificados | Não alterar globalmente para "corrigir" um footprint; validar especialmente U39 e U12. |
| Grade | 0,25 mm para mecânica; 0,05/0,10 mm para SMD | Manter conectores e furos alinhados à grade mecânica. |

### 2.2 Classes de rede

Criar as classes no **Board Setup → Design Rules → Net Classes** e atribuir as redes explicitamente. As larguras de potência são ponto de partida para o envelope documentado (até ~0,75 A na entrada, 0,5 A em +5 V e 1 A em +3V3); confirmar com a corrente medida e a elevação térmica real.

| Classe | Redes | Largura / clearance | Vias | Regras práticas |
| --- | --- | --- | --- | --- |
| `SIGNAL` | GPIO, I²C, UART, SWD, reset, pulsos, SPI interno | 0,20 / 0,20 mm | 0,30 / 0,60 mm | Uma face, sobre B.GND, vias só quando eliminarem cruzamento. |
| `SENSITIVE` | `WIND_DIR_ADC`, `AVREF_SENSOR`, cristais, `FB_3V3` | 0,25 / 0,25 mm | 0,30 / 0,60 mm | Curtas, afastadas de SW/RF; sem dividir o retorno de GND. |
| `PWR_3V3` | `+3V3`, `VCC_LORA` antes/depois de FB70 | 0,80 / 0,25 mm | 0,40 / 0,80 mm | Reduzir a 0,40 mm apenas no escape de pad; usar cobre largo ou zona no topo. |
| `PWR_5V` | `+5V` | 1,00 / 0,25 mm | 0,40 / 0,80 mm | Alimentar cargas por ramificações curtas, não por cadeia longa de pads. |
| `VIN` | entrada, `VIN_MAIN`, pós-fusível | 1,00 / 0,30 mm | 0,40 / 0,80 mm | Entre J/JP/F/Q/C/U60 usar cobre largo; 1,50 mm onde houver espaço. |
| `SWITCHING` | `Net-(U60-SW)`, `Net-(U61-SW)`, BST | 0,50 / 0,30 mm | evitar | Área mínima, sem zona, sem via e sem sinal em paralelo. |
| `RF_50R` | `RF_ANT_915` | **definir após calcular** | evitar | Largura não deve ser escolhida por tabela genérica; usar stack-up final e calculadora de impedância. |
| `FINE_ESCAPE` | apenas escape inevitável de U39 | 0,15 / 0,15 mm | não usar | Apenas dentro de poucos milímetros do encapsulamento; não tentar passar uma trilha entre pads de passo 0,40 mm. |

Não é necessário cobre de 2 oz neste primeiro protótipo, desde que as larguras acima, o plano de GND e as vias paralelas sejam respeitados. Se a medição mostrar corrente contínua acima do envelope ou aquecimento relevante, reabrir a decisão de cobre/stack-up antes de produzir lote.

## 3. Sequência obrigatória de execução

1. Resolver os avisos ERC que ainda pertencem ao esquemático, sincronizar PCB e esquemático e conferir referências, footprints e pinagens.
2. Criar contorno, furos, keep-outs, zonas de conector, plano B.GND e as classes acima.
3. Fixar primeiro conectores, furos, microSD, U.FL e a área RF; depois potência; depois MCU/clocks; por último sensores e passivos.
4. Posicionar cada capacitor antes de rotear sua carga. O caminho deve ser **pino de alimentação → capacitor → via/retorno GND**, e não capacitor ligado por uma trilha longa a uma linha já existente.
5. Rotear nesta ordem: loops dos bucks; RF; clocks/ADC; microSD; SPI LoRa; demais digitais; alimentação distribuída. Preencher zonas e executar DRC a cada etapa.
6. Inspecionar em 2D, 3D e Gerber. Só então gerar BOM/CPL/Gerbers e fazer revisão independente de polaridade, montagem e acesso mecânico.

## 4. Plano por folha do esquemático

### Folha raiz — integração e mecânica

**Componentes físicos:** H1–H4 e as interfaces hierárquicas; não há circuito ativo novo nesta folha.

- Usar a raiz para validar o mapa de conectores e a distribuição física dos blocos, não para criar um "barramento" de trilhas longas atravessando a placa.
- Marcar em `F.SilkS` a revisão da placa, nome do projeto, orientação da placa, `+`/`−` das entradas, identificador de cada conector e número de série/lote. Reservar uma área de 20 × 8 mm para etiqueta ou QR code, longe de furos e de RF.
- Colocar marca de pino 1 e identificação de função nos conectores, mesmo que a referência (`J30`) seja removida da serigrafia de montagem. O usuário de campo precisa ler `PLUV`, `ANEM`, `BIRUTA`, `PWR 12V`, `SD`, `DEBUG`.
- As referências dos componentes pequenos podem ir para `F.Fab`; na serigrafia, priorizar funções, polaridades, seleção de jumper e pontos de teste.

### Folha `alimentacao` — entrada 12 V, proteção e conversores

**Componentes:** J60, J61, JP60, F60, D60, Q60, D61, C60–C70, L60/L61, U60/U61, R60/R62/R63 e TP60–TP63.

- Posicionar J60 e J61 na borda. JP60 fica imediatamente depois, com as duas seleções impressas (`J61 1–2` e `P4 2–3`) e a advertência curta `UMA PONTE SOMENTE`. Deixar espaço para soldar e inspecionar o jumper.
- A ordem física deve acompanhar a ordem elétrica: entrada → JP60 → F60 → D60 → Q60 → C60/C61/C62/C63 → U60/L60/C65/C66 → +5V → U61/L61/C68/C69/C70 → +3V3. O TVS D60 fica perto do conector/pós-fusível, com trilha GND muito curta, larga e duas ou mais vias de GND adjacentes.
- U60, C61/C62/C63, L60, C65/C66 e o retorno GND formam um bloco compacto. O loop de entrada (`VIN–U60–GND–Cin`) e o loop de comutação (`SW–L60–Cout–GND–U60`) devem ter a menor área possível. Não passar `SW`, FB, RF, ADC ou trilha de sensor sob/ao lado do indutor.
- C64 deve encostar nos pinos BST/SW de U60. O nó `Net-(U60-SW)` só pode tocar U60, L60 e C64: sem via, sem test point, sem cobre de zona, sem trilha longa.
- Em U61, C67 encosta em VIN/GND; L61 e C68/C69/C70 encostam na saída; R62/R63 ficam junto ao pino FB. Buscar FB no lado positivo de C68–C70, após L61, por trilha fina e silenciosa; retornar R63 ao GND do CI, não ao retorno de potência depois do indutor.
- Separar fisicamente ao menos 10 mm de U8/U9 e evitar colocar U1 acima dos indutores. Prever cobre de dissipação sob os CIs apenas conforme datasheet/footprint, sempre conectado ao GND sólido no bottom por vias térmicas fora do nó SW.
- TP60, TP61, TP62 e TP63 devem ficar na borda da área de potência, acessíveis com a placa montada e etiquetados `VIN`, `5V`, `3V3`, `GND`. Não permitir que a ponta de prova toque `SW` por engano.

### Folha `mcu` — STM32, relógios, reset, SWD e bateria

**Componentes:** U4, Y2/Y3, C4/C5/C13/C14/C15/C16/C17–C24, FB1, BT1, SW1, J1–J3 e R6/R8/R10/R11.

- U4 fica no centro, orientado para que as trilhas de SPI para SD/LoRa e I²C para sensores saiam sem cruzamentos. Posicionar os conectores J2 (SWD) e J3 (UART) na borda, deixando espaço para o plugue; J1/BOOT0 próximo de SW1, mas sem bloquear o acesso.
- Colocar C4, C5, C13 e C14, um por pino/par VDD–VSS correspondente, no mesmo lado do MCU e a poucos milímetros dos pads. Cada um deve ter retorno GND por via ao lado do pad GND. C15 é o desacoplamento de +3V3 local; C16 (VCAP) deve ficar junto ao pino VCAP, sem dividir via/trilha com outro capacitor.
- FB1, C17 (1 µF) e C18 (10 nF) formam a alimentação analógica: FB1 entra pela +3V3; C17/C18 ficam do lado do MCU e junto de `AVREF_SENSOR`/VSSA. R50 da biruta deriva desta região. Não criar um plano AGND isolado: manter B.GND contínuo e apenas impedir que o retorno dos bucks atravesse esta área.
- Y3 com C19/C20 e Y2 com C21/C22 ficam imediatamente ao lado dos pinos de clock; os dois capacitores em pares simétricos e com retorno GND curtíssimo. Não rotear sinais, `SW`, SPI, LoRa ou alimentação de potência sob os cristais; o plano B.GND pode permanecer contínuo sob eles.
- C23 fica junto de VBAT/BT1. Manter BT1 afastada de fonte de calor e prever acesso para troca se ela for realmente necessária ao RTC. C24, R8 e SW1 formam o reset e ficam próximos ao pino NRST, com SW1 na borda acessível.
- Os pull-ups I²C R6/R11 devem ficar próximos ao MCU ou do tronco I²C, não ao final de um dos sensores. Rotear SDA/SCL juntas, curtas, sem paralelismo prolongado com `SW`.

### Folha `untitled` (BMP280) — sensor de pressão

**Componentes:** U8, C1/C2, R3/R4.

- Renomear a folha para `BMP280` no KiCad para eliminar ambiguidade documental. Posicionar U8 na face superior, perto de uma abertura protegida do gabinete e longe de J60, U60/U61, L60/L61, U12 e do DHT22.
- C1/C2 ficam junto dos pinos de alimentação do U8; R3/R4, que definem CSB/SDO, junto dos respectivos pinos. Usar trilhas I²C curtas de 0,20–0,25 mm sobre B.GND.
- Reservar janela de ar do gabinete sobre o orifício do sensor. Não cobrir o sensor com serigrafia, etiqueta, conformal coating ou adesivo. Não colocar via imediatamente sob a abertura/sensor, pois fluxo de ar, resíduos e aquecimento local degradam a leitura.
- Acrescentar `PRESSÃO — NÃO OBSTRUIR` na serigrafia próxima à abertura, se houver espaço.

### Folha `BH1750` — sensor de iluminância

**Componentes:** U9, C6/C25, R5/R9.

- U9 deve ficar na face superior, sob uma janela óptica do gabinete, com o eixo do encapsulamento orientado conforme a marca de pino 1. Deixar a região diretamente acima livre de texto, máscara adicional, parafuso, sombra de conectores e luz emitida por LEDs.
- C6/C25 ficam junto à alimentação/GND do sensor; R5 (endereço) e R9 ficam junto aos pinos correspondentes. Não estender o barramento I²C através da área RF ou do nó SW.
- Acrescentar uma marca de serigrafia `LUZ` e um contorno de zona proibida de etiqueta. A abertura óptica é requisito do gabinete, não substituível por furo aleatório na PCB.

### Folha `DHT22` — temperatura e umidade

**Componentes:** U1, C3 e R7.

- Se U1 estiver na mesma PCB, posicionar na borda superior/lateral de ar limpo, bem afastado de potência, MCU e principalmente reguladores/indutores. A própria massa térmica da placa altera a leitura: evitar cobre grande sob o sensor e fontes de calor próximas.
- Deixar o footprint THT e a área de inserção livres de componentes no raio de pelo menos 3 mm. C3 e R7 ficam do lado interno, próximos ao conector/pinos do DHT, mas não entre o corpo ventilado do sensor e a abertura do gabinete.
- Serigrafar `TEMP/UMID.` e a orientação do pino 1. O gabinete precisa de grelha com proteção contra água/poeira sem bloquear a convecção; confirmar isso antes de fixar a posição definitiva.

### Folha `Cartao SD` — microSD, proteção e buffer

**Componentes:** J17, U39, U11, C7–C10, R17/R18.

- J17 fica encostado à borda acessível, com a direção de inserção impressa e o envelope do cartão/ejeção transformado em `Keepout` para componentes. Conferir em 3D a altura e as pernas de fixação do conector.
- U39 fica imediatamente atrás de J17. Rotear primeiro o lado cartão (`SD_CLK_CARD`, `SD_CS_CARD`, `SD_DO_CARD`, DAT1/DAT2) entre J17 e U39, mantendo cada trecho curto e sem stubs; depois U11 do lado host. C7–C10 devem abastecer J17/U39 localmente, com via GND junto a cada capacitor.
- U11 fica entre U39 e U4, orientado para rotas diretas do SPI (`MCU_MOSI/MISO/SCK/CS`). Não colocar o microSD ou U39 sob a antena/keep-out de U12.
- O footprint U39 é de passo fino: validar orientação de pino 1, abertura de máscara, pasta e pads no visualizador Gerber. Sair pelos extremos dos pads; **não** rotear trilha entre pads nem usar via-in-pad. Usar `FINE_ESCAPE` somente quando inevitável e confirmar DRC/fabricabilidade.
- R17/R18 devem ficar junto a CS/CD conforme o esquema e os nomes funcionais `SD_CS` e `SD_CD` devem aparecer na serigrafia, não apenas as referências. Acrescentar um TP para GND ao alcance do osciloscópio se não houver um TP63 próximo.

### Folha `LORA Module` — rádio e RF de 915 MHz

**Componentes:** U12, J70, D70, FB70, C11/C12 e R14/R15.

- U12 ocupa a extremidade superior da placa. A antena é externa por J70/U.FL; reservar também espaço para o cabo coaxial sair sem dobrar sobre U12 ou sobre sensores. J70 deve estar a poucos milímetros de U12 e acessível após a montagem.
- `RF_ANT_915` deve ser uma única ligação curta, retilínea e sem via entre U12, D70 e J70. D70 é shunt de ESD: colocar seu pad GND encostado a pelo menos duas vias para B.GND, sem criar um stub longo no sinal RF.
- Antes de rotear RF, escolher o stack-up e a espessura final no pedido JLCPCB e calcular a largura de microstrip/CPWG de 50 Ω com a calculadora da fabricante. Não assumir que 0,20 mm é 50 Ω. Manter GND contínuo no bottom, cobre GND coplanar no topo conforme o cálculo e via fence de GND ao longo do trajeto (passo máximo 5 mm; mais denso junto a J70/D70). [Orientação JLCPCB sobre impedância controlada](https://jlcpcb.com/blog/pcb-layout-process-guide-2026-mastering-pcb-design).
- Manter área RF livre de indutores, cristal, microSD, trilhas digitais paralelas, cobre flutuante e metal do gabinete. Se futuramente for usada uma antena impressa/em módulo com antena integrada, o keep-out deve ser redesenhado pelo desenho do fabricante — não reutilizar automaticamente esta área.
- FB70 e C11/C12 ficam junto do pino VCC de U12; o retorno de C11/C12 recebe vias GND curtas. R14/R15 ficam junto dos pinos NSS/RST ou MCU, escolhendo a orientação que diminua cruzamentos. SPI e DIOs seguem juntos para U4 sobre plano contínuo e não passam sob o coaxial.
- Serigrafar `ANT 915 MHz`, `U.FL` e símbolo de não conectar/desconectar com a placa energizada. A antena externa, seu cabo e o gabinete fazem parte da validação RF/EMC.

### Folha `METEOROLOGIA` — agrupamento físico das interfaces

Esta folha é hierárquica e não contém componentes próprios. Ela define que as três interfaces externas devem formar uma faixa única próxima às RJ11 J30/J31/J32.

- Dispor J30 `PLUV`, J31 `ANEM` e J32 `BIRUTA` na mesma borda, em sequência coerente com o chicote. Deixar pelo menos 3 mm entre seus courtyards e espaço para pressão das travas dos plugs.
- Para cada conector, o caminho de entrada deve ser: pino de sinal → TVS primário → filtro/resistor → clamp → buffer/ADC. O primeiro TVS e seu retorno GND são a fronteira entre o cabo e a eletrônica; não levar o sinal bruto pelo centro da PCB.
- Manter a biruta, que é analógica, no extremo mais distante de L60/L61/U12. Os sinais digitais filtrados de chuva e vento podem seguir em paralelo curto, mas a entrada bruta e `WIND_DIR_ADC` não devem correr ao lado de `SW`, RF ou do barramento de entrada.
- Colocar pontos de teste em uma linha na borda interna do bloco, onde possam ser sondados sem desconectar o RJ11. Imprimir nome funcional abreviado ao lado de cada TP.

### Folha `PLUVIOMETRO` — entrada reed de chuva

**Componentes:** J30, D30/D31, R30–R32, C30/C31, U30 e TP30–TP33.

- J30 fica na borda; D30 deve ser o componente mais próximo do pino 4 do conector. Seu pad GND deve ligar imediatamente ao plano por duas vias de 0,30/0,60 mm ou uma via de potência, sem compartilhar trilha estreita com o retorno do sinal.
- R30, R31, C30, D31 e U30 devem seguir a topologia do esquema na ordem física. C30 encosta em R31/D31/U30, com via GND adjacente; C31 encosta no pino VCC de U30. R32 permanece próximo do conector/pino 3 conforme o esquema.
- Roteie `PLUVIOMETRO_RAW` apenas nesse bloco até D30/R30/R31; após o Schmitt, `RAIN_PULSE` pode ir diretamente para U4. Evitar via na entrada bruta e manter distância de 5 mm ou mais do nó SW/indutores.
- TP30 (`RAW`), TP31 (`FILT`), TP32 (`RAIN`) e TP33 (`GND`) devem estar em ordem elétrica e identificados. Silk adicional: `PLUV — RJ11 P3=GND / P4=SINAL`, condicionado à confirmação física da pinagem do cabo.

### Folha `ANEMOMETRO` — entrada reed de velocidade do vento

**Componentes:** J31, D40/D41, R40–R42, C40/C41, U40 e TP40–TP43.

- Repetir o mesmo padrão físico do pluviômetro: J31 → D40 com retorno de GND curtíssimo → R40/R41/C40/D41 → U40 → MCU. Componentes equivalentes devem ter a mesma orientação para reduzir erro de montagem/revisão.
- C40 implementa o filtro e C41 desacopla U40: colocar cada um junto à função correspondente e com via GND local. Não compartilhar uma única via de GND entre D40, C40 e C41.
- `WIND_SPEED_PULSE` sai de U40 para U4 por trilha curta de sinal. `ANEMOMETRO_RAW` não deve acompanhar a trilha de clock do microSD/LoRa.
- Pontos TP40–TP43 e silk seguem o padrão `RAW`, `FILT`, `WIND`, `GND`. Adicionar `ANEM — RJ11 P3=GND / P4=SINAL` somente após teste do conector/cabo real.

### Folha `BIRUTA` — entrada analógica de direção do vento

**Componentes:** J32, D50/D51, R50–R52, C50 e TP50–TP53.

- J32 e D50 ficam na borda como as outras entradas. Posicionar R50/R51/R52, D51 e C50 em linha em direção ao MCU, com `WIND_DIR_ADC` terminando próximo do pino ADC de U4.
- `WIND_DIR_RAW` deve ser curtíssimo entre J32/D50/R50/R51 e não passar sob U12, L60/L61 ou microSD. C50 e R52 ficam junto ao ADC; a via GND de C50 é adjacente ao capacitor. A netlist atual lista C50 como **100 nF**, enquanto a documentação anterior cita 10 nF: confirmar o valor final no esquemático antes de roteá-lo e antes de comprar.
- `AVREF_SENSOR` sai da região silenciosa de FB1/C17/C18, via R50, sem passar pelo retorno de potência. A entrada analógica não precisa de plano AGND separado; precisa de plano B.GND íntegro e caminho sem corrente pulsante comum.
- Pontos TP50 (`RAW`), TP51 (`ADC`), TP52 (`VREF`) e TP53 (`GND`) ficam acessíveis. Silk adicional: `BIRUTA — RJ11 P3=+3V3 / P4=SINAL / P5=GND`, após confirmação de pinagem do sensor real.

## 5. Plano de GND, zonas e uso de vias

- Criar uma zona `GND` em **B.Cu por toda a placa**. Ela é a referência de retorno de sinais e não deve ser cortada por trilhas. Não dividir GND em AGND/DGND/potência; o isolamento funcional vem de posicionamento e retorno local, não de fendas no plano.
- Criar zona de GND em F.Cu apenas onde ela não estrangular rotas nem formar ilhas. Repreencher e remover ilhas ao final. O plano inferior continua prioritário.
- Toda mudança de camada de uma trilha de sinal deve ter uma via GND de costura próxima ao ponto de transição, quando houver espaço. Isso preserva o caminho de retorno de alta frequência.
- Usar vias GND próximas aos pads GND de desacoplamentos, TVS, U12/J70, U60/U61 e nos conectores. Para TVS de cabo e RF, duas vias próximas são preferíveis a uma via afastada.
- Via stitching no perímetro: espaçamento de 8–10 mm é suficiente como padrão; reduzir para 3–5 mm ao redor da área RF e da entrada de cabo. Respeitar 0,50 mm de cobre até a borda e não criar anéis sem função sob sensores ambientais.
- Para atravessar corrente entre faces, usar no mínimo três vias de potência em paralelo para +3V3/+5V e quatro no caminho de entrada quando a transição for inevitável. Não usar via em pad sem processo de preenchimento/tampamento contratado.

## 6. Regras de EMC, integridade e boas práticas

- Sinais digitais devem ficar sobre B.GND contínuo. Nunca roteá-los sobre abertura, gargalo ou divisão no plano, porque o retorno terá de contornar a fenda e aumentará a emissão.
- Minimizar área de loop, não apenas comprimento de trilha. Isso é crítico nos loops U60/U61, TVS de conectores, desacoplamentos, clock e RF.
- Manter distância inicial de 5 mm entre qualquer trilha de sensor/ADC e os indutores/nós SW; aumentar se o roteamento permitir. Não rotear sinal sensível paralelo a `SW`, `VIN_MAIN` ou RF.
- Usar curvas de 45° ou arcos; não criar ângulos agudos, cobre órfão ou stubs. Nos sinais lentos deste projeto não há requisito de length matching; a prioridade é retorno contínuo, rotas curtas e separação funcional.
- Não passar trilhas abaixo de U.FL, do caminho RF, do microSD de ejeção, das aberturas ópticas/ambientais ou dos cristais. Não colocar cobre no keep-out de uma antena que o fabricante indicar.
- Colocar o TVS na fronteira do cabo e o clamp secundário perto da eletrônica protegida; conectar seus retornos diretamente ao plano. A proteção de ESD não qualifica a placa contra raio ou instalação externa sem proteção coordenada no cabo/quadro.
- Manter silkscreen a pelo menos 0,20 mm dos pads expostos e remover textos que o CAM possa recortar. Conferir sobreposição de serigrafia no DRC antes de gerar Gerber.
- Em `F.SilkS`, marcar: referência/revisão; pino 1 de U4/U11/U12/U39; polaridade de J60/J61/BT1/C60/D60/D61; seleção JP60; orientação de inserção do microSD; antena; nomes dos RJ11; pontos de teste; e `NÃO OBSTRUIR` nas janelas de BMP/BH/DHT quando aplicável.

## 7. Checklist de revisão antes de encomendar

- [ ] ERC do esquemático sem erros não justificados e discrepâncias de valor resolvidas (em especial C50).
- [ ] Todos os footprints, pino 1, polaridades, pinagens de J60/J30–J32 e furos de fixação confrontados com datasheet e peça física.
- [ ] `Edge.Cuts` fechado; dimensões, furos, janela de sensores, espaço de cartão e altura dos componentes validados no 3D e, idealmente, no gabinete.
- [ ] B.Cu é GND contínuo; nenhuma trilha crítica cruza abertura de plano; zonas refeitas sem ilhas de cobre.
- [ ] Cada desacoplamento está junto do pino de seu CI e tem retorno curto ao plano; loops dos dois bucks inspecionados manualmente.
- [ ] `RF_ANT_915` possui largura de 50 Ω calculada para o stack-up final, rota curta sem via e área RF/antena respeitada.
- [ ] DRC sem erros, sem itens desconectados e sem violações de silk/courtyard aceitas sem justificativa explícita.
- [ ] Gerbers abertos em visualizador independente; revisar máscara, pasta, placas de referência, contorno, furos PTH/NPTH e a camada B.Cu de GND.
- [ ] BOM e CPL gerados da mesma revisão do `.kicad_pcb`; disponibilidade e política de montagem JLCPCB conferidas na data do pedido.
- [ ] Primeiro protótipo testado com fonte limitada: curto entre rails, 10/12/16 V, ripple, temperatura, microSD, LoRa, sensores e suscetibilidade ao conectar cabos.

## 8. Informações que ainda precisam de confirmação

O layout pode começar com estas regras, mas estas decisões devem ser fechadas antes de congelar a posição dos componentes:

1. Desenho do gabinete: posição de conectores, furos, janelas de BMP/BH/DHT, altura máxima e se haverá metal próximo da antena.
2. Tipo, modelo, comprimento e orientação da antena 915 MHz; isso define o keep-out e a geometria final de `RF_ANT_915`.
3. Pinagem física confirmada dos três cabos/RJ11 e se os sensores ficarão sujeitos a descargas/surtos além do previsto no esquema.
4. Corrente real máxima em +5 V e +3V3, além de perfil de transmissão LoRa e escrita no SD; estes dados confirmam as larguras e a necessidade de cobre de 2 oz.
5. Se JLCPCB montará a placa ou se a montagem será manual. Isso define acabamento, fiduciais/painel, orientação de componentes e a necessidade de pedir montagem THT específica.
