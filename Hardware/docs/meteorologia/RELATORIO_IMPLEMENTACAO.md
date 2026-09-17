# Sensores meteorológicos — registro de implementação

Registro: 17/09/2026. Projeto: `Hardware/esquematico_V4.kicad_pro`, KiCad 9.

## Escopo e estado

Este conjunto de alterações implementa o **esquemático** das interfaces do
pluviômetro, anemômetro e biruta, suas conexões à Nucleo-F446RE e os campos de
componentes/footprints. Não representa uma liberação para fabricação.
O arquivo `esquematico_V4.kicad_pcb` não foi atualizado nesta implementação:
sincronização, posicionamento, roteamento e revisão mecânica permanecem pendentes.
O firmware não foi alterado.

## Organização e integração

A folha principal aponta para `meteorologia.kicad_sch`, que contém as folhas
`pluviometro.kicad_sch`, `anemometro.kicad_sch` e `biruta.kicad_sch`.
Todos esses arquivos ativos ficam diretamente em `Hardware/`; hierarquia elétrica
não exige subpastas no disco. As cópias antigas na pasta
`Sensores_meteorologicos/` não são a hierarquia ativa.

Foram retirados da raiz os circuitos meteorológicos antigos e a representação
duplicada U7 da Nucleo. A interface ativa é U5, na folha `mcu.kicad_sch`.
A biblioteca local `Hardware_MCU` representa os conectores Morpho CN7/CN10,
exporta AVDD como `VREF_SENSOR` e conecta AGND ao GND.
Também foi uniformizada a grafia da rede `GND` nas folhas afetadas.

| Sensor | Conector | Sinal ao MCU | Pino Morpho | GPIO |
| --- | --- | --- | --- | --- |
| Pluviômetro | J30 | RAIN_PULSE | CN7_30 | PA1 |
| Anemômetro | J31 | WIND_SPEED_PULSE | CN10_19 | PC7 |
| Biruta | J32 | WIND_DIR_ADC | CN7_28 | PA0 / ADC |
| Referência do divisor | — | VREF_SENSOR | CN10_7 | AVDD da Nucleo |

Cada RJ11 6P6C usa pino 3 como retorno GND e pino 4 como sinal; os demais estão
marcados sem conexão. Essa atribuição deve ser conferida com multímetro no
conjunto físico, considerando a orientação de visualização do plugue.

## Circuitos implementados

### Pluviômetro e anemômetro

O contato reed fecha o sinal do cabo contra GND. Há proteção ESD primária no
sinal, pull-up de 10 kΩ para 3,3 V **antes** do resistor série de 4,7 kΩ,
capacitor de filtro para GND, clamp secundário e buffer Schmitt não inversor.
Essa posição do pull-up evita formar um divisor resistivo que elevaria o nível
baixo com o reed fechado. A saída preserva a borda de descida usada pelo firmware.

| Função | Pluviômetro | Anemômetro |
| --- | --- | --- |
| ESD primário | D30 | D40 |
| Pull-up 10 kΩ | R30 | R40 |
| Série 4,7 kΩ | R31 | R41 |
| Filtro | C30: 100 nF | C40: 10 nF |
| Clamp secundário | D31 | D41 |
| Schmitt | U30 | U40 |
| Desacoplamento 100 nF | C31 | C41 |
| Pontos de teste | TP30–TP33 | TP40–TP43 |

As constantes de tempo nominais são 1,47 ms na abertura e 0,47 ms no fechamento
para chuva; 147 µs e 47 µs para vento. Esses valores não substituem a verificação
de repiques e duração mínima dos pulsos nos sensores reais.

### Biruta

R50, de 10 kΩ, liga `VREF_SENSOR` ao sinal do sensor, formando o divisor com a
resistência da biruta. Após a proteção ESD D50, o sinal passa por R51 de 10 kΩ,
clamp D51, R52 de 1 kΩ e chega ao ADC. C50 de 10 nF filtra a entrada do ADC.
O clamp superior liga-se a +3V3, não diretamente à referência analógica.
TP50–TP53 permitem medir sinal bruto, ADC, referência e GND.

Os resistores série não alteram o divisor ideal em corrente contínua, mas a
corrente de fuga dos diodos/ADC e o tempo de aquisição introduzem erros reais.
Usando 120 kΩ como maior resistência nominal do sensor, a resistência equivalente
vista pelo capacitor é aproximadamente 20,23 kΩ e a constante de tempo é 202 µs.
Validar aquisição, tolerâncias e temperatura antes de fechar os limiares da LUT.

## Componentes e footprints cadastrados

Os códigos abaixo identificam as peças selecionadas; não são garantia de estoque
reservado, preço ou disponibilidade de montagem na JLCPCB na data da compra.

| Peça | Código LCSC | Referências | Footprint KiCad |
| --- | --- | --- | --- |
| Ckmtw R-RJ11R06P-A800 | C189750 | J30, J31, J32 | Sensores_Meteo:RJ11-TH_PCB-6P6C-90 |
| Nexperia PESD5V0S1UL,315 | C85402 | D30, D40, D50 | Diode_SMD:D_SOD-882 |
| Nexperia BAS70-04,215 | C389351 | D31, D41, D51 | Package_TO_SOT_SMD:SOT-23 |
| TI SN74LVC1G17DBVR | C7836 | U30, U40 | Package_TO_SOT_SMD:SOT-23-5 |
| UNI-ROYAL 0603WAF1002T5E, 10 kΩ, 1% | C25804 | R30, R40, R50, R51 | Resistor_SMD:R_0603_1608Metric |
| UNI-ROYAL 0603WAF4701T5E, 4,7 kΩ, 1% | C23162 | R31, R41 | Resistor_SMD:R_0603_1608Metric |
| UNI-ROYAL 0603WAF1001T5E, 1 kΩ, 1% | C21190 | R52 | Resistor_SMD:R_0603_1608Metric |
| YAGEO CC0603KRX7R9BB104, 100 nF, X7R, 50 V | C14663 | C30, C31, C41 | Capacitor_SMD:C_0603_1608Metric |
| FH 0603B103K500NT, 10 nF, X7R, 50 V | C57112 | C40, C50 | Capacitor_SMD:C_0603_1608Metric |
| Pad de teste de 1,5 mm | — | TP30–33, TP40–43, TP50–53 | TestPoint:TestPoint_Pad_D1.5mm |

Os pontos de teste são cobre na PCB, sem componente comprado na BOM. Os símbolos
contêm os campos de fabricante, MPN, LCSC, especificação e função. A biblioteca
`Sensores_Meteo` mantém o conector importado e o símbolo local BAS70-04.

Referências dos fabricantes:
[SN74LVC1G17](https://www.ti.com/lit/ds/symlink/sn74lvc1g17.pdf),
[BAS70-04](https://assets.nexperia.com/documents/data-sheet/BAS70-04.pdf),
[PESD5V0S1UL](https://assets.nexperia.com/documents/data-sheet/PESD5V0S1UL.pdf).
No BAS70-04 os pinos são 1=ânodo externo, 2=cátodo externo e 3=junção dos diodos;
no circuito, respectivamente GND, +3V3 e sinal protegido.

## Verificação registrada

Os resultados em `validacao/` foram produzidos pelo KiCad 9.0.6 em 16/09/2026.

| Verificação | Resultado registrado |
| --- | --- |
| ERC anterior | 540 ocorrências |
| ERC após alterações | 28 ocorrências no projeto completo |
| ERC meteorologia, três sensores e MCU | 0 ocorrências nessas folhas |
| DRC anterior da PCB | 53 violações e 164 conexões não roteadas |

O arquivo `netlist.xml` registra as conexões exportadas. Na revisão pré-commit
foram conferidos os pinos 3/4 dos RJ11, orientação dos diodos primários,
alimentação e saída dos buffers, divisor da biruta e chegada dos sinais ao MCU.
ERC sem ocorrências não comprova desempenho analógico, imunidade ou segurança.

As 28 ocorrências restantes estão na raiz (9), alimentação (3), BMP280 (2),
BH1750 (1), cartão SD (2) e LoRa (11). Incluem alimentação sem fonte reconhecida,
saídas de alimentação em conflito, rótulos pendentes e tipos elétricos não
definidos. Devem ser analisadas antes da fabricação, sem apenas suprimir avisos.

## Pendências e critérios para liberar a PCB

- Sincronizar o esquemático com a PCB, resolver componentes antigos/duplicados,
  posicionar e rotear. O DRC anterior não é aprovação do layout novo.
- Revisar mecanicamente o RJ11 importado contra o desenho do fabricante e uma
  amostra. Os dois furos de fixação vieram como `thru_hole`, com diâmetro do pad
  igual ao furo; confirmar se são pinos plásticos e devem ser NPTH. O courtyard
  também precisa contemplar a extensão desses furos. Não considerar o footprint
  mecanicamente liberado apenas porque a numeração elétrica foi conferida.
- Colocar proteção ESD junto aos conectores, com retorno GND curto; filtros,
  clamps e desacoplamento próximos aos circuitos protegidos. Manter o sinal
  analógico afastado de LoRa, relógios e comutação da alimentação.
- Validar surtos e alimentação desligada. O PESD de 5 V não limita sozinho a
  entrada a 3,3 V; os clamps podem injetar corrente na alimentação. Esta solução
  não possui isolamento e não está qualificada contra raios ou falhas contínuas.
- Medir a resistência da biruta em todas as posições. O firmware ativo tem LUT
  de 16 entradas; `num_samples=8` é média de amostras, não número de direções.
  A decisão de apresentar somente oito direções permanece no firmware/calibração.
- Rever o debounce do anemômetro: 20 ms impõe teto aproximado de 50 Hz.
  Com 2,4 km/h/Hz, 50 m/s exigiria 75 Hz. O filtro de hardware não remove essa
  limitação de software. Confirmar a constante com o sensor real.
- Validar o fator de chuva e o debounce de 200 ms com o pluviômetro físico.
  Confirmar também o tempo de aquisição e relógio do ADC com esta impedância.
- Conferir BOM, equivalentes, estoque JLCPCB/LCSC, montagem THT e orientação dos
  encapsulamentos antes de encomendar. Gerar Gerbers/BOM/CPL somente após revisão
  global ERC/DRC e aprovação mecânica e elétrica.

As imagens em `esquematicos/` são exportações para consulta. Os arquivos
`.kicad_sch` são a fonte editável; scripts temporários de migração e salvamentos
automáticos não fazem parte da documentação liberada neste commit.
