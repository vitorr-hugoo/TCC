# Registro de desenvolvimento de hardware — TCC

> Atualizado em 02/10/2026. Este registro consolida as decisões, correções e validações feitas nesta sessão. O esquema atual do KiCad é a fonte de verdade; anotações e versões anteriores servem somente como histórico. As decisões posteriores de implementação e de roteamento estão detalhadas em [DECISOES_IMPLEMENTACAO_PCB_2026-10.md](DECISOES_IMPLEMENTACAO_PCB_2026-10.md).

## Escopo e convenções adotadas

- A placa deixa de depender da Nucleo: o controlador da PCB é o **STM32F446RET6** em encapsulamento LQFP-64.
- O projeto foi organizado por folhas funcionais: alimentação, MCU/depuração, sensores ambientais, interfaces meteorológicas, cartão microSD e rádio LoRa.
- Componentes de compra receberam fabricante, MPN, LCSC, ficha técnica e footprint. O modelo 3D é conferido no footprint; furos de fixação, pads de teste e jumper de solda são exceções mecânicas/PCB e não possuem modelo 3D aplicável.
- As anotações em roxo que apenas repetiam dados de componentes foram removidas. Permanecem as legendas que explicam o comportamento elétrico, a finalidade de proteções e decisões de integração.

## 1. Alimentação

### Entrada, seleção e proteção

- A entrada aceita a fonte selecionada por `JP601`: conector DC ou JST-XH. A seleção evita a ligação simultânea das fontes.
- `F601` limita a corrente de falha; `D601` (TVS) absorve transientes na entrada.
- `Q601` é o MOSFET de proteção contra inversão de polaridade. `R601` define o estado de gate e `D602` limita \(V_{GS}\), protegendo a porta do MOSFET. O zener **não regula** a tensão de alimentação após o MOSFET; sua função é somente limitar a tensão gate–source.
- O banco de capacitores da entrada foi mantido com funções complementares: o eletrolítico/MLCC de maior valor fornece energia em baixa frequência e os capacitores menores reduzem a impedância em frequências mais altas, inclusive a corrente pulsante dos conversores buck.

### Conversor implementado

- A arquitetura de dois estágios foi substituída. `VIN_MAIN` alimenta diretamente o buck fixo de 3,3 V `U601`, **AP63203WU-7** (Diodes Inc., LCSC `C780769`), com faixa de entrada de 3,8 a 32 V e saída nominal de até 2 A.
- `L601` foi substituído por **ANR5040T3R9M**, 3,9 µH (APV, LCSC `C7427132`), no footprint `Inductor_SMD:L_APV_ANR5040`. A folha registra o uso de capacitores de entrada e saída no laço recomendado pelo fabricante.
- O segundo buck e sua realimentação foram removidos da folha ativa: `U602`, `L602`, `R602`, `R603` e o ponto de teste de `+5V`. Não há barramento funcional `+5V` na revisão atual.
- `C609–C611` permanecem no domínio `+3V3`: são desacoplamento/bulk da carga e não pertenciam ao estágio removido. Eles não são substituídos pelos capacitores locais do regulador, pois atuam em posições elétricas distintas.
- A alimentação `+3V3` continua sendo a referência lógica do MCU, sensores, SD e LoRa. A confirmação em bancada de ripple, temperatura e margem de corrente do novo estágio ainda é obrigatória antes da fabricação.

## 2. Interfaces meteorológicas

### Anemômetro e pluviômetro

- Foi corrigida a interpretação do sensor: no RJ11, o pino 3 fornece +3V3 e o pino 4 é o sinal. Internamente o reed switch conecta **VCC → reed → sinal** quando o ímã passa.
- O condicionamento externo passou a fornecer a referência correta, filtro RC, proteção de linha, resistor série para limitar corrente de surtos e clamp Schottky antes do Schmitt trigger.
- Foi documentado que `D401/D402` (e equivalentes do pluviômetro) limita o sinal aos trilhos, enquanto o TVS na entrada trata transientes vindos do cabo. O resistor série de 1 kΩ reduz a corrente que chegaria ao clamp/entrada durante um evento de ESD ou ligação indevida.
- A escolha entre 10 nF e 100 nF no filtro RC foi registrada: 10 nF prioriza preservação de pulsos curtos; 100 nF faz filtragem/debounce mais agressivo. O valor final deve ser confirmado em ensaio com a frequência máxima real do anemômetro e o comprimento do cabo.
- A consequência no firmware também foi registrada: a borda contada deve corresponder ao nível ativo após o novo condicionamento. O firmware não foi alterado nesta sessão.

### Biruta (direção do vento)

- O sensor Misol foi tratado como rede resistiva interna de oito reeds: RJ11 pino 2 é o sinal analógico e pino 5 é GND; o pull-up externo de 10 kΩ fornece a excitação do divisor.
- `R501` atua como pull-up da rede do sensor. `R502`/`C501` filtram o sinal para o ADC e `R503` limita corrente para a entrada/proteções. A seleção de `C501` deve equilibrar estabilidade de leitura e tempo de acomodação ao trocar a direção.
- Foram preservados proteção contra ESD e clamp de sinal antes do ADC.
- Os pinos 3 e 4 do conector permanecem explicados como passagem reservada para anemômetro/pluviômetro. Como essa passagem não é usada nesta PCB, o pino reservado foi marcado explicitamente como **não conectado**; isso evita apresentar uma falsa interligação entre folhas.

## 3. DHT22

- Foi mantido o DHT22/AM2302 alimentado em +3V3, com resistor de pull-up SMD e capacitor de desacoplamento de 100 nF junto ao sensor.
- A página foi padronizada com os demais blocos quanto a referências de terra, campos de compra e footprints/modelos.
- O símbolo, footprint e modelo usados foram integrados às bibliotecas locais do repositório, em vez de depender de bibliotecas pessoais do KiCad.

## 4. MCU, relógios, RTC e depuração

### Alimentação e referência analógica

- Todos os pinos `VDD` do STM32F446RET6 foram ligados a +3V3 com desacoplamento distribuído (100 nF por região e capacitores de reserva).
- `VDDA` recebe +3V3 através de `FB101`, com `C107` de 1 µF e `C108` de 10 nF para desacoplamento analógico. O nome elétrico único dessa rede é `AVREF_SENSOR`; ele alimenta `VDDA` e é a referência dos sinais analógicos dos sensores.
- Não foi criada uma ilha `AGND`: `VSSA` e o restante do circuito usam o plano de GND comum, reservando boa colocação e caminho de retorno curto para o domínio analógico.
- `VCAP_1` usa `C106` de 4,7 µF para GND. **VCAP não recebe +3V3**, pois é a saída do regulador interno do núcleo do STM32.

### Osciladores

- `Y102`: HSE de 8 MHz, cristal SMD 3,2 × 2,5 mm, Suzhou Liming `3225-8.00-10-10-10/A` (LCSC `C518154`), com `C109/C110` de 15 pF C0G. Ele permite atingir as frequências de operação usuais do STM32F446 por PLL; após a fabricação o clock pode ser reduzido no CubeMX/firmware.
- `Y101`: LSE de 32,768 kHz, SJK `7LC32768F07UC` (LCSC `C252399`), com `C111/C112` de 9 pF C0G. O LSE mantém o RTC com baixo consumo e boa precisão quando o sistema principal está parado ou em baixa potência.
- No layout, cristais e respectivos capacitores devem ficar muito próximos aos pinos, sem trilhas rápidas, planos partidos ou cobre indevido sob a região sensível.

### Reset, boot e RTC

- `NRST`: pull-up de 10 kΩ, capacitor de 100 nF e botão de reset.
- `BOOT0`: pull-down de 10 kΩ e header de 2,54 mm para forçar +3V3 apenas quando for necessário entrar no bootloader.
- A alimentação do domínio de backup usa `BT101` com célula **CR1225** e suporte horizontal **MYOUNG BS-12-B4AK025-R** (LCSC `C20606807`), mais `C113` de 100 nF em `VBAT`. O footprint e o modelo STEP locais são `Hardware_Battery:BT_Holder_MYOUNG_BS-12-B4AK025-R`; a mudança elimina o volume incompatível do suporte vertical de CR2032.

### Interfaces de programação e diagnóstico

- `J102` é o header SWD de 1 × 6, 2,54 mm: +3V3, GND, SWDIO, SWCLK, NRST e SWO. JTAG completo não foi adotado.
- `J103` é o header UART de diagnóstico: +3V3, TXD e RXD. Ele foi pensado para um adaptador USB–UART externo; o adaptador deve operar em lógica de 3,3 V e compartilhar GND com a PCB. A alimentação deve vir de apenas uma fonte definida para evitar back-powering.
- Os sinais dos sensores, LoRa, SD, I²C e UART foram mapeados aos pinos do STM32. A configuração de periféricos e GPIO fica como tarefa de firmware posterior.

## 5. BMP280 e BH1750

- As folhas foram comparadas com os circuitos recomendados pelos fabricantes, deixando de copiar cegamente os módulos de protoboard.
- Os dois sensores usam +3V3, desacoplamento local e I²C com pull-ups adequados.
- No BH1750, o Schottky de alimentação foi removido: ele reduziria a tensão útil sem oferecer benefício neste circuito. O pulldown de `ADDR` foi ajustado de 1 kΩ para 10 kΩ; `ADDR` não pode flutuar, pois define o endereço I²C.
- O capacitor de entrada redundante do módulo foi removido onde não era necessário, preservando o desacoplamento local correto.
- Os componentes foram migrados de PTH para footprints SMD e receberam MPN, LCSC, datasheet e modelos/footprints válidos.
- A decisão final de integração do BH1750 deixou de ser montá-lo na PCB principal: ele será uma **placa filha externa**, conectada por JST-XH de quatro vias e I²C3. O detalhamento elétrico, mecânico e de firmware está em [DECISAO_BH1750_PLACA_FILHA_I2C3_DHT22.md](DECISAO_BH1750_PLACA_FILHA_I2C3_DHT22.md).

## 6. Cartão microSD

- O buffer `SN74LVC125APWR` foi mantido, conforme decisão do projeto, para condicionar os sinais SPI.
- `DAT1` e `DAT2` foram deixados como no-connect no modo SPI. Portanto, não recebem pull-up; são necessários apenas para operação SDIO de 4 bits, que não é usada aqui.
- A proteção `U802` EMIF06-MSD02N16 foi colocada entre buffer e socket. Os pinos `*_IN` ficam do lado do controlador/buffer e os `*_EX` ficam do lado do socket. Em SPI: CS/CMD/CLK seguem para o cartão e DAT0/MISO retorna do cartão para o buffer.
- O socket é `J801`, **SOFNG TF-15x15** (LCSC `C111196`), push-push SMT, com detecção. O footprint foi importado da fonte EasyEDA/LCSC e preservado localmente como `Hardware_SD_Interface:microSD_SOFNG_TF-15x15_C111196`, acompanhado pelo modelo `microSD_SOFNG_TF-15x15_C111196.step`.
- O símbolo de `J801` corresponde à mecânica real: pads 1–8 são os contatos do cartão, 9 é `SD_CD/DET_B`, 10 é GND/`DET_A` e 11–13 são abas de blindagem em GND. A referência, footprint e modelo 3D foram conferidos após a atualização da PCB.
- Foram criadas as bibliotecas locais `Hardware_SD_Interface` (símbolos persistentes do buffer e socket) e respectivo footprint/modelo. Isto evita que futuras atualizações das bibliotecas globais do KiCad alterem a semântica SPI usada no ERC.
- O nó de GND dos capacitores locais que estava fora da grade foi ajustado de 215,6676 mm para 215,9 mm, sem mudança de conectividade.

## 7. Rádio LoRa

- O bloco usa o módulo LoRa1276-C1 de 915 MHz, evitando reproduzir o circuito RF interno do transceptor na placa principal.
- Foi escolhida antena externa por conector U.FL/I-PEX: `J701` recebe o sinal `RF_ANT_915`; `D701` fornece proteção ESD RF de baixa capacitância.
- `FB701` isola a alimentação do módulo e `C701/C702` fazem desacoplamento local.
- A interface usa SPI e os sinais DIO foram mapeados: DIO0 em PA11, DIO1 em PA12 e DIO2 em PB0. A definição final de interrupções deve ser refletida no firmware.
- Símbolo, footprint, conector U.FL e modelos STEP estão em `Hardware_LoRa`, com bibliotecas declaradas no projeto.

## 8. Bibliotecas, cadastro e validação

Bibliotecas locais relevantes:

| Biblioteca | Conteúdo principal |
| --- | --- |
| `Hardware_Battery` | suporte horizontal CR1225 MYOUNG e modelo 3D local |
| `Hardware_LoRa` | módulo 915 MHz, U.FL e proteção RF |
| `Hardware_SD` | filtro/proteção EMIF06 |
| `Hardware_SD_Interface` | socket SOFNG TF-15x15, buffer SPI e STEP do socket |
| `Hardware_Power` | componentes da alimentação |
| `Hardware_Sensors`, `DHT22`, `Sensores_Meteo` | sensores e conectores das interfaces |

Verificações registradas até 02/10/2026:

- O ERC estava sem erros após a criação da interface BH1750. O ERC/DRC devem ser executados novamente como verificação de liberação depois do roteamento; este registro não substitui essa validação final.
- A netlist foi gerada com sucesso após a atualização do socket microSD. A verificação específica de `J801` não encontrou violação DRC associada à nova footprint; a PCB ainda tem itens não roteados, portanto não está liberada para fabricação.
- Auditorias anteriores de campos, footprints e modelos 3D permanecem como base histórica. As três referências alteradas depois dessa auditoria — `U601`/`L601`, `BT101` e `J801` — receberam MPN, LCSC, datasheet, footprint e modelo 3D locais compatíveis.
- O arquivo de exportação `hardware/kicad/estacao-meteorologica/estacao-meteorologica.csv` ainda lista `R602` e `R603` da arquitetura antiga. Ele deve ser **regenerado a partir do esquemático ativo** antes de cotar ou fabricar; não é a fonte de verdade desta revisão.

## 9. Pendências antes da fabricação

1. Validar em bancada a alimentação direta `VIN_MAIN` → `+3V3` sob a pior carga prevista, incluindo temperatura de `U601`, ripple, partida e margem de corrente. `U602` não existe mais na folha ativa.
2. Testar anemômetro e pluviômetro com cabo real, verificando debounce, ruído e frequência máxima antes de fixar o tratamento de borda no firmware.
3. Criar uma tabela medida de tensão ADC × direção para a biruta e implementar janelas/tolerâncias no firmware.
4. No CubeMX/firmware, habilitar HSE/LSE, RTC com VBAT, SWD, UART de depuração, SPI do SD/LoRa, I²C e os GPIO/ADC dos sensores conforme os rótulos do esquema. Em particular, configurar DHT22 em PA9 e I²C3 em PA8/PC9 em uma branch de firmware separada.
5. Concluir posicionamento e roteamento conforme [DECISOES_IMPLEMENTACAO_PCB_2026-10.md](DECISOES_IMPLEMENTACAO_PCB_2026-10.md): plano contínuo de GND em B.Cu, regiões térmica/RF/sensíveis, larguras por trecho e pontos de teste acessíveis. Confirmar em 3D o conjunto CR1225, microSD, U.FL e furos M3 no gabinete.
6. Regenerar BOM/netlist, executar ERC e DRC finais e só então confirmar estoque e preço LCSC no momento da compra, pois disponibilidade não é permanente.
