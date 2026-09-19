# Alimentação da PCB principal — referência da implementação KiCad

Revisão PWR-1 · Projeto `esquematico_V4` · KiCad 9.0.6 · pesquisa em 18/09/2026.

## 1. Escopo e estado da entrega

Circuito discreto implementado em `Hardware/alimentacao.kicad_sch`, com a declaração da folha e as redes +5V, +3V3 e GND na raiz. O antigo módulo LM2596 e seus conectores de distribuição foram substituídos. Não foi feito layout, atualização da PCB a partir do esquema, roteamento ou geração de arquivos de fabricação.

Este é um **esquemático para desenvolvimento e prototipagem, não uma liberação de fabricação**. Correntes reais, transitórios, estabilidade com todos os módulos, aquecimento dentro do quadro e compatibilidade mecânica ainda precisam ser verificados. O restante do projeto contém pendências elétricas anteriores a esta implementação.

**Pendência atual:** a entrada externa de 5 V da Nucleo ainda precisa ser movida para E5V na folha MCU, mediante a autorização solicitada. A alimentação isoladamente tem ERC limpo, mas a integração completa não está liberada. Ver [resultado legível da validação](validacao/VALIDACAO.md).

Arquivos de apoio:

- [Esquema vetorial da alimentação](esquematicos/esquematico_V4-alimentacao.svg) — ampliar para ler pinos e valores.
- [Prévia PNG](esquematicos/esquematico_V4-alimentacao.png).
- [BOM, consulta de disponibilidade e auditoria de bibliotecas](BOM_e_bibliotecas.md).
- [Validação automatizada](validacao/resumo_validacao.json) e [ERC completo](validacao/erc_depois.json).
- [Consulta pública JLCPCB arquivada](catalogo_jlc_final.json), contendo também alternativas rejeitadas. Somente a BOM indica as peças montadas.

## 2. Arquitetura e fronteiras entre placas

```text
FORA DA PCB PRINCIPAL
Painel / rede auxiliar → placa de energia → VRLA, carregador, LVD
                              │
                         BUS nominal 12 V
                              │ dois fios: positivo + retorno
DENTRO DA PCB PRINCIPAL        ▼
J61 (placa de energia) ─── pad 1
                              JP60 — pad 2 → F60 → Q60 → VIN_MAIN
J60 (fonte P4 12 V) ───── pad 3               │             │
                                            D60            └→ U60 → +5V → U61 → +3V3
                                             │                       │            │
                                            GND                   cargas 5V    cargas 3V3
```

D60 está depois de F60 e antes de Q60. O zener D61 limita gate-source de Q60; os capacitores de entrada ficam após Q60. O diagrama acima é funcional; a pinagem autoritativa está no arquivo KiCad e na netlist exportada.

A placa externa continua responsável por carregamento VRLA, prioridade solar, rede de emergência, corte por descarga profunda, proteção junto à bateria e limitação de corrente no chicote. BQ24650 e TPS3700 pertencem a essa placa externa, não a esta folha.

Condição de projeto: **10–16 V contínuos no conector selecionado**, nominal 12 V, fonte SELV e corrente limitada a no máximo 2 A. A faixa atende o barramento VRLA previsto, mas não autoriza painel solar direto, rede elétrica, fonte de 24 V ou bateria sem proteção. A capacidade de entrada do CI U60 não amplia automaticamente a especificação de toda a placa.

## 3. Etapa A — conectores e seleção manual

| Item | Ligação e função |
|---|---|
| J60 | Jack DC-005-2.0: pino 1 centro positivo; pino 2 retorno; pino 3 chaveado sem conexão |
| J61 | JST-XH de 2 vias: pino 1 positivo da placa de energia; pino 2 GND |
| JP60 | Ponte de solda com três pads; pad 2 alimenta F60 |
| TP63 | Referência GND para medições |

Configuração de JP60, sempre com as fontes desligadas:

- **1–2 fechados:** usar J61; positivo do P4 fica isolado.
- **2–3 fechados:** usar P4; positivo de J61 fica isolado.
- **Tudo aberto:** a placa não recebe alimentação.
- **Nunca fechar 1–2 e 2–3 simultaneamente:** isso coloca as fontes em paralelo.

JP60 é fabricado aberto. A conexão deve ser feita na configuração final e identificada na serigrafia. Não existe OR automático, prioridade por firmware ou troca automática de fonte nesta PCB; essa função pertence à placa de energia. Os dois GNDs permanecem comuns, portanto a seleção não produz isolamento galvânico.

J60 foi selecionado pelo código exato do catálogo e convertido do EasyEDA. Antes de fabricar, conferir desenho dimensional, recortes/furos oblongos, orientação, pinagem com multímetro e encaixe do plugue real. A denominação informal “P4” não garante todas as dimensões. Referência do conector: [desenho HanElectricity distribuído pela LCSC](https://datasheet.lcsc.com/datasheet/pdf/0b943d4c409056643475de1b2f68984a.pdf?productCode=C22459640).

## 4. Etapa B — proteção e filtragem de entrada

| Componentes | Função e motivo da ligação |
|---|---|
| F60, 1,5 A lento | Fusível em série, antes do TVS e da eletrônica; proteção local complementar |
| D60, SMBJ16CA | TVS bidirecional entre entrada pós-fusível e GND; limita transientes positivos e negativos sem curto permanente na inversão de polaridade de 12 V |
| Q60, DMP6023LE-13 | PMOS de proteção contra polaridade invertida: drenos 2 e 4 voltados ao conector; source 3 voltada ao buck; gate 1 controlado por R60/D61 |
| R60, 100 kΩ | Puxa o gate para GND e estabelece VGS negativo quando a entrada está correta |
| D61, BZT52C10 | Cátodo no source, ânodo no gate; limita a diferença de potencial gate-source |
| C60, 47 µF/35 V | Reserva local de energia; ESR do eletrolítico contribui para amortecer ressonâncias do cabo |
| C61/C62, 10 µF/50 V cada | Capacitores cerâmicos de entrada; fornecer corrente pulsante de U60 com baixa impedância |
| C63, 100 nF/50 V | Desacoplamento de alta frequência, próximo de VIN/GND de U60 |
| TP60 | Medição de VIN_MAIN após a proteção |

Em polaridade correta, o diodo de corpo de Q60 permite a partida e o gate baixo estabelece condução pelo canal. Em polaridade invertida, o arranjo bloqueia a alimentação da carga. **Um único PMOS não impede corrente reversa em todas as condições:** não alimentar os barramentos de saída por outra fonte, USB ou instrumento sem analisar os caminhos de retorno.

Dimensionamento preliminar:

- Exemplo a 0,75 A: perda em Q60 ≈ I²R = 0,75² × 0,028 = **16 mW**; queda ≈ 21 mV. Esse RDS(on) é especificado a VGS = −10 V; recalcular para temperatura e VGS reais.
- R60: adotando VZ ≈ 10 V apenas como aproximação, corrente ≈ (VIN − VZ)/100 kΩ: 20 µA em 12 V e 60 µA em 16 V. O zener é especificado em corrente muito maior; esses valores não são previsão precisa do consumo. Medir VGS e corrente em repouso.
- F60: resistência típica 0,14 Ω → perda aproximada de **79 mW a 0,75 A**. Conferir curva tempo-corrente e derating térmico antes da montagem definitiva.
- Energia de carga dos 67 µF nominais de entrada a 16 V: E = CV²/2 ≈ **8,6 mJ**. Isso não é um cálculo de pico de hot-plug: indutância do cabo, ESR, capacitância efetiva e impedância da fonte determinam a oscilação.
- D60: VRWM de 16 V e clamp de catálogo de 26 V na condição especificada do fabricante. A comparação com U60 (32 V de operação/35 V absoluto) fornece margem estática, mas **não comprova imunidade a surto**. O pico local pode aumentar por indutância e forma de onda.

Limitações importantes: F60 tem capacidade de interrupção especificada de 50 A, não é um fusível geral para qualquer bateria VRLA. A bateria precisa de fusível adequado junto ao terminal e a saída externa precisa de proteção/limitação. O TVS não protege contra sobretensão contínua, fonte errada, falha arbitrária do buck ou descarga atmosférica direta. A corrente de fonte limitada a 2 A não garante abertura rápida de um fusível de 1,5 A; não usar essa combinação como crowbar de sobretensão. Se esses eventos fizerem parte do requisito, acrescentar proteção coordenada específica na placa externa ou revisar esta entrada.

Fontes: [DMP6023LE, pinagem e limites](https://www.diodes.com/datasheet/download/DMP6023LE.pdf); [UMW SMBJ, tabela do TVS](https://www.umw-ic.com/static/pdf/62a30f50b229ed3e3eee960a7e35a088.pdf); [SART, fusível S1206-S](https://datasheet.lcsc.com/datasheet/pdf/e14cebd5b8cf04e322288dba224b0dfb.pdf?productCode=C553929); [Nichicon, capacitores de alumínio SMD, série WT](https://www.nichicon.co.jp/english/products/pdf_x/Chip_Type_Aluminum_Electrolytic_Capacitors_E.pdf).

O BZT52C10 selecionado é da Hongjiacheng, especificado em 350 mW e 9,5–10,5 V a 5 mA; não transferir automaticamente os limites de outro fabricante que utilize o mesmo nome de família. [Folha técnica do fabricante distribuída pela LCSC](https://datasheet.lcsc.com/lcsc/2501071555_hongjiacheng-BZT52C10_C19077408.pdf).

## 5. Etapa C — barramento variável para 5 V

U60 é AP63205WU-7, buck síncrono com saída fixa de 5 V. A alternativa TPS62163 foi pesquisada, mas o AP63205 reduz o preço do CI e oferece maior margem de tensão na entrada. Não é substituto pino a pino: símbolo, footprint e circuito foram implementados especificamente para o AP63205.

| Pino de U60 | Conexão implementada |
|---|---|
| 1, FB | Sense de +5V após L60, sem divisor externo |
| 2, EN | VIN_MAIN; partida autônoma |
| 3, VIN | VIN_MAIN com C61/C62/C63 próximos no layout |
| 4, GND | Plano de retorno comum |
| 5, SW | L60 e um terminal de C64 |
| 6, BST | Outro terminal de C64, 100 nF; não ligar diretamente a VIN ou GND |

L60 = 4,7 µH; C65/C66 = 22 µF/25 V cada; TP61 mede a saída. O circuito base segue as figuras 1/21 e a tabela de componentes do [datasheet Diodes](https://www.diodes.com/datasheet/download/AP63200-AP63201-AP63203-AP63205.pdf). O [guia oficial AP63205-EVB](https://www.diodes.com/assets/Evaluation-Boards/AP63205-EVB-User-Guide.pdf) fornece referência adicional de implementação física; não foi copiada a placa de avaliação como módulo.

Cálculos de primeira ordem em condução contínua, sem perdas:

```text
D ≈ VOUT / VIN
ΔIL ≈ VOUT × (1 − VOUT/VIN) / (L × f)
Ipk ≈ IOUT + ΔIL/2
```

Com 16 V → 5 V, 4,7 µH e 1,1 MHz: D ≈ 0,3125 e ΔIL ≈ **0,665 A**. Com L 20% menor e frequência 6% menor, ΔIL ≈ **0,884 A**. Não inclui toda variação com corrente/temperatura. Isat mínimo declarado de L60 é 7,2 A e sua corrente térmica mínima é 5 A, acima do envelope proposto. O tamanho maior desse indutor compra margem e baixa resistência; pode ser reduzido futuramente após medir a carga.

Com Cout efetivo hipotético de 22 µF (50% do nominal), ripple capacitivo ideal ≈ ΔIL/(8fC) = **3,4 mV** no caso nominal. ESR, layout, modo de baixa carga e transientes não estão nessa estimativa: ela não é garantia de ripple da placa. A capacitância real dos MLCC deve ser conferida sob tensão DC.

Os 2 A são classificação do CI, não garantia da placa e não corrente disponível simultaneamente para cargas 5 V e 3,3 V. A carga do próximo estágio também consome desse barramento.

## 6. Etapa D — 5 V para 3,3 V

U61 é TLV62569DBVR, buck síncrono SOT-23-5. Foi escolhido em lugar de AMS1117/7805 ou LDO de baixa corrente para reduzir dissipação durante transmissão LoRa e escrita em microSD, mantendo preço baixo. Não oferece PG nesta versão e não precisa de comunicação com o STM32.

| Pino de U61 | Conexão implementada |
|---|---|
| 1, EN | +5V; habilita automaticamente |
| 2, GND | Retorno comum |
| 3, SW | L61, 2,2 µH, e somente o pequeno nó chaveado |
| 4, VIN | +5V; C67 de 10 µF/25 V próximo ao CI |
| 5, FB | Junção R62/R63/C70, não diretamente em +3V3 |

R61 = 300 kΩ e R62 = 150 kΩ formam 450 kΩ no braço superior; R63 = 100 kΩ é o inferior. C70 = 6,8 pF fica entre +3V3 e FB, em paralelo ao conjunto R61+R62. C68 = 10 µF/25 V é a capacitância local de saída; TP62 é o ponto de teste. A dupla 300 kΩ + 150 kΩ usa valores Basic consultados, evitando depender de um único valor menos comum.

```text
VOUT = 0,6 × (1 + (300k + 150k)/100k) = 3,300 V
Idivisor = 3,3 / 550k = 6,0 µA
```

Com resistores de 1% nos extremos opostos, apenas o erro resistivo resulta em aproximadamente **3,246–3,355 V**. Somar erro da referência, linha/carga, temperatura e ripple para o orçamento completo; não tratar como faixa garantida de saída.

Para 5 V → 3,3 V, L = 2,2 µH e f típico = 1,5 MHz: ΔIL ≈ **0,34 A** e Ipk ≈ **1,17 A** a 1 A médio. A frequência desse conversor é dependente das condições; o cálculo é nominal. L61 escolhido: Isat mínimo 4 A, Irms mínimo 3,8 A, DCR máximo 58 mΩ. Valores da [série Sunlord MWSA-S, tabela técnica do fabricante hospedada no TI E2E](https://e2e.ti.com/cfs-file/__key/communityserver-discussions-components-files/196/MWSA_2D00_S-series-of-SMD-Power-Inductor_2D00_1.pdf).

**Capacitância total importa:** a netlist já contém 30,5 µF nominais em outras folhas de +3V3. Com C68, são 40,5 µF visíveis no esquema. C69 de 22 µF foi deixado **DNP**: montá-lo sem nova análise elevaria o total a 62,5 µF, fora da faixa usada como referência para este estágio. Capacitores internos de módulos e chicotes não modelados devem entrar na revisão de estabilidade. C68 precisa ficar perto de U61 mesmo havendo capacitores nas cargas.

Circuito e critérios LC: [TI TLV62569, seção 8.2, tabela 4 e seção 10](https://www.ti.com/lit/ds/symlink/tlv62569.pdf). A TI também alerta sobre exceder a combinação ensaiada em [revisão técnica de capacitância de saída](https://e2e.ti.com/support/power-management-group/power-management/f/power-management-forum/1294590/tlv62569-schematic-review). Os capacitores locais C67/C68 foram reduzidos para 0805/25 V para economizar área e custo; os de entrada de alta tensão permanecem em 1206/50 V. [Especificação Samsung CL21A106KAYNNNE](https://mm.digikey.com/Volume0/opasdata/d220001/medias/docus/5545/CL21A106KAYNNNE%20Spec.pdf).

## 7. Orçamento de corrente e baixo consumo

Envelope inicial **a validar**, não consumo medido: até 0,5 A de cargas diretamente em 5 V e até 1 A em 3,3 V. Não assume que a estação consumirá tudo isso.

```text
I5V_total ≈ I5V_cargas + 3,3 × I3V3 / (5 × η3V3)
Ientrada ≈ (5 × I5V_cargas + 3,3 × I3V3/η3V3) / (VIN × η5V)
```

Exemplo com eficiências assumidas de 90% em ambos os bucks: I5V_total ≈ **1,23 A**, potência na entrada ≈ **6,85 W**, corrente ≈ **0,685 A em 10 V**. Com 85%, a estimativa sobe para cerca de 0,75 A. Eficiências são hipóteses de cálculo, não resultados de simulação/ensaio.

As correntes quiescentes típicas dos CIs (22 µA e 35 µA nas respectivas condições de datasheet) **não são o consumo total em repouso da placa**. Somar divisor, gate/zener, fuga do eletrolítico/TVS, perdas de conversão, sensores e a Nucleo. ST-LINK, LEDs e regulador da Nucleo podem dominar o consumo. A alimentação nova não transforma sozinha a estação em ultra-low-power.

Medir repouso, aquisição, LoRa TX e escrita no SD, incluindo picos. Para cada modo: energia = potência × tempo; somar Wh no ciclo de um dia. Somente então dimensionar bateria/painel, incluindo eficiência, profundidade de descarga permitida, temperatura, envelhecimento e dias sem sol. Essa escolha continua pendente na placa de energia externa.

## 8. Integração com as folhas existentes

Na folha BH1750, com autorização do usuário, foi removido U10 (L7805) e seus três ramais terminais. Ele colocava uma saída nominal de 5 V na rede +3V3. C3 continua desacoplando +5V, C6 continua em +3V3 e o restante da circuitaria do sensor foi preservado. Não foi feita revisão funcional completa dessa interface.

Nucleo: o caminho correto para alimentação externa é E5V, CN7 pino 6, não CN7 pino 18. A implementação dessa correção depende da autorização específica solicitada para a folha MCU; conferir o status final no relatório de validação. Na placa física, configurar JP5 em 2–3/E5V, retirar JP1 conforme a revisão do hardware e ligar a alimentação externa antes do USB. CN7 pino 18 é saída +5V e não deve unir as duas fontes. A rede +3V3 desta PCB não está ligada à saída do regulador 3V3 da Nucleo. Fonte: [ST UM1724, seções 7.5.2–7.5.4, revisão 17](https://www.st.com/resource/en/user_manual/um1724-stm32-nucleo64-boards-mb1136-stmicroelectronics.pdf).

Não há lógica de controle/telemetria de energia adicionada ao MCU. Os dois ENs ficam ativos conforme a alimentação disponível. UVLO interno dos bucks não substitui LVD para proteger a VRLA.

## 9. Regras para o futuro layout

1. Conectores, JP60, F60, D60 e Q60 perto da borda de entrada, nessa ordem elétrica. Retorno do TVS curto e largo, sem atravessar o terra dos sensores.
2. C61/C62/C63 junto a VIN/GND de U60; reduzir a área do loop de comutação. C60 perto da entrada protegida, não na ponta de uma trilha fina.
3. C64 encostado nos pinos BST/SW. Manter a área de SW pequena e afastada de feedback, RF, antena e sinais analógicos.
4. L60 e C65/C66 perto do buck. Sense de FB retirado do lado de saída de L60, preferencialmente junto ao positivo dos capacitores, sem compartilhar queda da trilha de potência.
5. Para U61, C67 no par VIN/GND; L61/C68 próximos; R61/R62/R63/C70 junto a FB e longe de SW. Retorno de R63 ao terra silencioso do CI.
6. Plano GND contínuo e largura/vias dimensionadas para corrente e temperatura reais. Não aplicar largura arbitrária nem dividir o retorno criando gargalos. Consultar os layouts oficiais acima.
7. Marcar +/− dos conectores e as duas posições mutuamente exclusivas de JP60. Confirmar áreas de courtyard, altura de C60 e acesso para retrabalho do jumper.
8. Fazer revisão independente símbolo–pad–pinagem, DRC, inspeção 3D e teste de encaixe de J60 antes de encomendar. A importação EasyEDA e a coincidência de números de pads não certificam mecanicamente a peça.

## 10. Verificação e ensaios antes de liberar fabricação

A validação automática compara redes pino a pino, isolamento das entradas com JP60 aberto, polaridade dos componentes, EN/FB/BST, DNP, BOM e numeração de pads. Também compara cada ocorrência ERC com a linha de base por tipo, folha e UUID dos itens. Os PWR_FLAG apenas declaram redes alimentadas para o ERC; não foram usados para suprimir conflito real de reguladores.

O registro final está em `validacao/resumo_validacao.json`. O projeto completo não deve ser apresentado como “ERC limpo”: persistem erros e avisos anteriores em outras folhas. O arquivo `.kicad_pcb` não foi alterado nesta tarefa.

Sequência de bancada recomendada:

1. Sem energia, conferir curto entre VIN/+5V/+3V3/GND, seleção única de JP60, orientação de Q60/D61/C60 e continuidade dos conectores.
2. Testar a alimentação sem Nucleo e sem módulos, com fonte limitada e carga pequena. Medir TP60/61/62/63. Aumentar o limite gradualmente conforme a corrente esperada; limite muito baixo pode causar partida repetitiva.
3. Testar 10, 12 e 16 V; depois variar cargas separadas e combinadas até o envelope proposto. Confirmar tensões, ripple com ponta de terra curta, resposta a degrau e recuperação de curto controlado.
4. Medir temperatura dos CIs, indutores, fusível e conectores na pior temperatura ambiente prevista; estimar temperatura de junção com metodologia adequada ao layout. Não confundir proteção térmica com operação aceitável contínua.
5. Medir hot-plug com o cabo final, inrush, ringing, VGS de Q60, picos em VIN/SW e margem das tensões máximas. Ensaio de inversão somente com fonte de bancada limitada e procedimento controlado, nunca começando pela bateria.
6. Só então integrar Nucleo e demais cargas, verificar configuração de USB e observar LoRa/SD simultâneos. Considerar back-power pelos GPIOs durante partidas e desligamentos assimétricos.
7. Medir consumo durante o ciclo real e calcular autonomia. Reconferir estoque/quantidades/taxas JLCPCB, DNP e modelos dos fornecedores antes de comprar.

Repetição dos testes de arquivos, na raiz do repositório:

```powershell
& 'C:\Program Files\KiCad\9.0\bin\kicad-cli.exe' sch export netlist --format kicadxml -o Hardware/docs/alimentacao/validacao/netlist.xml Hardware/esquematico_V4.kicad_sch
& 'C:\Program Files\KiCad\9.0\bin\kicad-cli.exe' sch erc --format json -o Hardware/docs/alimentacao/validacao/erc_depois.json Hardware/esquematico_V4.kicad_sch
python -X utf8 Hardware/tools/validar_alimentacao.py
```

Os scripts de migração em `Hardware/tools/` registram como a alteração foi feita; **não executar novamente o gerador inicial sobre o esquema já revisado**. Os arquivos KiCad são a fonte de verdade para edições posteriores.

## 11. Comparação de alternativas pesquisadas

Preços unitários indicativos do catálogo, sem taxas de montagem ou compra mínima; ver snapshot e links na BOM.

| Função | Seleção implementada | Alternativa examinada | Critério adotado |
|---|---|---|---|
| Buck 5 V | AP63205WU-7, C2071056, US$ 0,4216 | TPS62163DSGR, C97534, US$ 1,9222 | Menor preço do CI, entrada de maior tensão e montagem TSOT; a comparação não inclui taxas PCBA |
| 3,3 V | TLV62569DBVR, C141836, US$ 0,0795 + LC/divisor | MCP1700T-3302E/TT, C39051, US$ 0,4004 | Buck favorece eficiência sob carga; o LDO de 250 mA continua interessante se as medições futuras mostrarem consumo e picos suficientemente baixos |
| Inversão | DMP6023LE-13, C154901, US$ 0,5844 | AO3401A, C15127, US$ 0,0943 | O PMOS de 60 V custa/ocupa mais, mas melhora a margem frente a transientes e inversão com capacitores ainda carregados; o de 30 V não foi adotado |
| C67/C68 | 0805/25 V, C15850 | 1206/50 V, C13585 | Os 50 V são mantidos apenas na entrada de maior tensão; o par local menor economiza cerca de US$ 0,36 nos preços consultados |

Não existe uma escolha universalmente ótima antes de medir o ciclo de consumo e conhecer quantidade/serviço de montagem. Esta revisão privilegia protótipo montável, margem para picos e disponibilidade consultável, sem pretender corrente de repouso mínima absoluta. A documentação anterior deve ser lida com estas substituições: não comprar TPS62163, MCP1700 ou AO3401A para os footprints desta BOM.
