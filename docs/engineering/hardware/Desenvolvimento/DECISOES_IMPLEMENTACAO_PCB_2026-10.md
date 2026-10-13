# Decisões de implementação da PCB — 02/10/2026

## Finalidade e regra de leitura

Este adendo registra as decisões tomadas depois dos estudos iniciais, principalmente as que já alteraram o projeto KiCad ou que guiam o posicionamento e o roteamento ainda pendentes. Ele não substitui o esquemático/PCB ativos em `hardware/kicad/estacao-meteorologica`: esses arquivos são a fonte de verdade elétrica e mecânica.

Os estados usados neste documento são:

- **Implementado** — já refletido no esquemático, bibliotecas ou PCB ativos.
- **Diretriz de layout aprovada** — deve ser aplicada durante o roteamento; não significa que a trilha já exista.
- **Pendente de validação** — decisão feita, mas ainda dependente de ensaio, firmware ou revisão final.

Os documentos de decisão de [BH1750](DECISAO_BH1750_PLACA_FILHA_I2C3_DHT22.md), [DHT22/BMP280](DECISAO_DHT22_ABRIGO_TERMICO_BMP280.md) e o [estudo de alimentação](ESTUDO_ALIMENTACAO_12V_PARA_3V3_SEM_ESTAGIO_5V.md) continuam válidos como justificativa técnica. As seções “estado atual” deste arquivo prevalecem quando houver diferença entre o estudo e a implementação posterior.

## 1. Alimentação: conversão direta para 3,3 V

**Estado: implementado no esquemático; pendente de validação em bancada.**

- A topologia aprovada é `VIN_MAIN` → `U601` → `+3V3`; não há estágio funcional intermediário de `+5V`.
- `U601` é o buck fixo **AP63203WU-7** (Diodes Inc., LCSC `C780769`), adequado ao envelope de entrada de 3,8 a 32 V e à saída de 3,3 V.
- `L601` é o indutor **ANR5040T3R9M**, 3,9 µH (APV, LCSC `C7427132`), no footprint `Inductor_SMD:L_APV_ANR5040`.
- `U602`, `L602`, `R602`, `R603` e o ponto de teste do barramento de 5 V foram retirados da folha ativa. Um nome, rótulo, pad ou trilha residual de `+5V` não deve ser recriado sem uma carga real que exija esse barramento.
- `C609–C611` foram mantidos em `+3V3`. São bulk/desacoplamento do barramento de carga e não substituem os capacitores que fecham o laço local do buck; os dois grupos devem permanecer próximos de suas respectivas funções.

Antes da compra, é obrigatório regenerar a BOM/netlist. A exportação CSV existente ainda contém `R602` e `R603`, que pertencem à topologia histórica e não à folha ativa.

## 2. Backup do RTC: CR1225

**Estado: implementado.**

Para reduzir altura e área ocupada, o conjunto de backup mudou de CR2032 vertical para:

| Item | Definição final |
| --- | --- |
| Célula | CR1225, 3 V |
| Suporte | MYOUNG `BS-12-B4AK025-R` |
| LCSC | `C20606807` |
| Footprint local | `Hardware_Battery:BT_Holder_MYOUNG_BS-12-B4AK025-R` |
| Modelo 3D local | modelo STEP correspondente em `Hardware_Battery.3dshapes` |

O footprint foi criado a partir do arquivo correto do componente e a associação de `BT101` foi atualizada. A verificação mecânica final deve conferir a abertura do abrigo, a face de montagem e o acesso para troca da célula.

## 3. Cartão microSD: socket e biblioteca locais

**Estado: implementado; pendente de DRC global após roteamento.**

O socket selecionado não é o Molex utilizado em uma revisão antiga. A referência atual é:

| Item | Definição final |
| --- | --- |
| Socket | SOFNG `TF-15x15`, push-push SMT com detecção |
| LCSC | `C111196` |
| Footprint local | `Hardware_SD_Interface:microSD_SOFNG_TF-15x15_C111196` |
| Modelo 3D local | `microSD_SOFNG_TF-15x15_C111196.step` |

O footprint e o modelo foram importados da fonte EasyEDA/LCSC para a biblioteca local e usados em `J801`. O símbolo acompanha o item físico: contatos 1–8 do cartão, pino 9 de detecção, pino 10 de detecção/terra e abas 11–13 em GND. A footprint foi reposicionada/rotacionada preservando a relação dos sinais no board.

Ao atualizar essa footprint, a restrição global `min_hole_clearance` foi estabelecida em **0,20 mm**. Isso comporta a folga nominal de aproximadamente 0,2005 mm do furo mecânico do socket; não deve ser aumentada sem verificar novamente a footprint do `C111196`.

## 4. Sensores ambientais e organização no abrigo Stevenson

### BH1750

**Estado: interface da placa principal implementada; placa filha e firmware pendentes.**

- O BH1750 não será soldado dentro do abrigo junto da PCB principal. Ele será instalado em **placa filha externa**, sob domo/difusor apropriado, para medir iluminância incidente de modo útil como grandeza meteorológica.
- A ligação da placa principal é `J221` JST-XH 1×4: pino 1 GND, pino 2 `+3V3`, pino 3 `BH_I2C3_SCL`, pino 4 `BH_I2C3_SDA`.
- O cabo I²C recebe proteção ESD no conector. Os pull-ups de 4,7 kΩ permanecem na folha do MCU, próximo à origem do barramento, como no I²C1; não foram incluídos resistores série/DNP.
- O barramento escolhido é I²C3 em PA8/PC9. Para liberar PA8, o dado do DHT22 foi deslocado para PA9. A configuração de GPIO/I²C deve ser feita em branch de firmware separada, sem alterar esta etapa de PCB.

### DHT22 e BMP280

**Estado: decisão de posicionamento aprovada; roteamento pendente.**

- A PCB principal ficará vertical no centro do abrigo: entrada e conversores na parte inferior; LoRa e sensores na parte superior.
- O corpo ventilado do DHT22 deve ultrapassar a borda superior da PCB, voltado para as aberturas do abrigo. A solução reduz a influência direta da massa de cobre e dos componentes, mesmo sem uma placa auxiliar para o DHT22.
- O BMP280 deve ficar na região superior, próximo ao DHT22 e afastado dos conversores, mas sem obstruir suas portas de pressão. A temperatura do BMP280 é diagnóstico térmico/compensação interna de pressão, **não** um offset automático para a leitura do DHT22.
- As aberturas ao redor do abrigo reduzem, mas não eliminam, aquecimento por condução, radiação e pluma térmica da PCB. A validade final depende de comparação em bancada/campo com referência externa.

### Sensores de vento

**Estado: arquitetura elétrica aprovada; confirmar montagem/conectores no fechamento do layout.**

O conjunto de referência permite o anemômetro conectado à biruta: RJ11 com pino 3 em `+3V3` e pino 4 como sinal do anemômetro; a biruta utiliza pino 2 como sinal analógico e pino 5 como GND. Essa passagem é viável desde que a identificação dos pinos, a proteção de entrada e o condicionamento de cada sinal se mantenham independentes. A decisão não transforma o sinal do anemômetro em analógico nem dispensa suas proteções/filtro digital.

## 5. Regras de fabricação e net classes configuradas

**Estado: configurado no arquivo de projeto; aplicar conscientemente durante o roteamento.**

Premissas de fabricação: PCB de duas camadas, FR-4 de 1,6 mm, cobre de 1 oz em F.Cu e B.Cu. As restrições globais ativas são largura mínima de trilha **0,20 mm**, clearance cobre–cobre **0,20 mm**, cobre–Edge.Cuts **0,50 mm** e folga de furo **0,20 mm**.

As classes abaixo representam a configuração atual. `PWR_5V` foi mantida como classe de reserva, porém não existe uma alimentação funcional `+5V` nesta revisão.

| Classe | Largura / clearance | Via (diâmetro / furo) | Uso |
| --- | --- | --- | --- |
| `SIGNAL` | 0,20 / 0,20 mm | 0,60 / 0,30 mm | GPIO, I²C, UART, SPI e sinais digitais condicionados |
| `SENSITIVE` | 0,25 / 0,20 mm | 0,60 / 0,30 mm | ADC de biruta, referência analógica e cristais |
| `PWR_3V3` | 0,80 / 0,20 mm | 0,80 / 0,40 mm | tronco de +3V3 |
| `PWR_5V` | 1,00 / 0,25 mm | 0,80 / 0,40 mm | reserva, sem rede funcional atual |
| `VIN` | 1,00 / 0,20 mm | 0,80 / 0,40 mm | entrada e `VIN_MAIN` |
| `SWITCHING` | 0,50 / 0,30 mm | 0,60 / 0,30 mm | nó SW/BST do buck; evitar via |
| `FINE_ESCAPE` | 0,20 / 0,20 mm | sem via | apenas escapes inevitáveis e curtos |
| `RF_50R` | não definido | sem via | `RF_ANT_915`; definir após cálculo de impedância da fabricante |

`FINE_ESCAPE` não deve ser usado como regra geral para tornar o roteamento mais fácil. Embora o plano inicial previsse 0,15 mm, a configuração persistida na revisão documentada está em **0,20 mm** e sem via; portanto ela não deve ser interpretada como autorização automática para fabricar 0,15 mm. Se um escape de 0,15 mm se tornar inevitável, ajustar conscientemente a classe depois de confirmar a capacidade do fornecedor. A largura de RF também não deve ser arbitrada: depende do stackup real, espessura do dielétrico e geometria coplanar/microstrip.

## 6. Diretriz de larguras: tronco, ramificação e pads pequenos

**Estado: diretriz de layout aprovada.**

A net class define um padrão de roteamento, não obriga entrar em um pad pequeno com a largura do tronco. O objetivo é manter a seção larga onde há corrente compartilhada e estreitar apenas no trecho terminal:

| Trecho | Largura orientativa |
| --- | --- |
| `VIN_MAIN` e caminho de entrada | 1,00 mm, sem gargalos e com transições robustas |
| Tronco da saída do buck até bulk de +3V3 | 0,80 mm |
| Distribuição curta de +3V3 | 0,50 mm |
| Entrada em VDD de MCU e componentes pequenos | 0,25 mm; 0,20 mm somente se inevitável |
| Desacoplamento de 100 nF | 0,20–0,25 mm, curto e direto entre pino e GND |
| `VBAT` | 0,25 mm |
| `VCAP_1` e `C106` | 0,25–0,30 mm, curtíssimo, sem ramificação nem via |
| Cristais HSE/LSE e capacitores associados | 0,20–0,25 mm, curtos, simétricos quando aplicável e sem via |

`C105` é bulk de `+3V3`: use 0,80 mm no lado conectado ao tronco do regulador e 0,50 mm quando ele alimentar uma ramificação. Não confundir `C105` com `C106`; `C106` é o capacitor exclusivo de `VCAP_1` do STM32.

## 7. Aterramento, vias e conectores PTH

**Estado: diretriz de layout aprovada.**

- **B.Cu será plano contínuo de GND.** F.Cu será usado prioritariamente para sinais, potência, pequenos trechos de GND e, se necessário, zonas que não fragmentem o retorno inferior.
- Um capacitor pequeno de desacoplamento pode usar **uma via normal 0,60/0,30 mm** imediatamente ao lado do pad GND. A distância e a área do laço importam mais do que aumentar arbitrariamente o diâmetro da via.
- `C106`/`VCAP_1` é ainda mais crítico: uma ligação direta, curta e dedicada ao plano, sem compartilhamento e sem via no caminho do sinal de VCAP.
- Para o bulk de `+3V3` (`C105`), usar duas vias normais em paralelo ou uma via de potência 0,80/0,40 mm quando necessário para acessar o plano. Para os capacitores de entrada/saída do buck, usar duas a quatro vias de potência agrupadas e muito próximas do laço de comutação.
- Um pad PTH de GND já atravessa F.Cu e B.Cu pelo barril metalizado. Vias extras só são úteis para reduzir impedância em corrente relevante ou costurar a área de retorno; quatro vias em série ao lado de qualquer conector não são automaticamente melhores.
- Ao trocar uma trilha de sinal entre faces, inserir uma via de costura de GND próxima quando isso preservar o caminho de retorno. Em U.FL, blindagens e bordas RF, usar múltiplas vias de GND/fence.
- Headers PTH de passo 2,54 mm podem ser montados em qualquer face conforme a mecânica. A solda normalmente fica na face oposta ao corpo do conector, mas o barril é elétrico nas duas faces. Sempre que possível, sair do pad com sinal em F.Cu e preservar B.Cu como plano de GND; não há obrigação elétrica de rotear o sinal pela face em que a solda aparece.

## 8. Pontos de teste

**Estado: padrão de footprint implementado para sinais; posicionamento final depende do roteamento.**

O padrão adotado para sinais de debug é `TestPoint:TestPoint_Pad_D1.0mm`, um pad SMD circular de 1,0 mm. Ele reduz a área em relação aos pontos antigos e é adequado para ponta fina de multímetro/osciloscópio. Os pontos de potência principais — `VIN_MAIN`, `+3V3`, GND e, se existir apenas por necessidade de teste histórico, `+5V` — devem manter área de contato suficiente para garra/ponta de corrente; não reduzir sua acessibilidade só para compactar a PCB.

Critério de colocação: o pad de teste deve ser uma **ramificação curta** da rede, e não uma interrupção obrigatória da trilha principal. Posicioná-lo em local acessível depois de montar conectores, SD e módulos; evitar planos de comutação, região RF, trilhas de cristal e borda mecânica.

## 9. Pendências para liberar fabricação

1. Terminar o placement respeitando a divisão térmica inferior/sensores e RF superior.
2. Rotear usando as diretrizes das seções 5–7 e manter B.Cu contínuo como referência de retorno.
3. Definir a largura de `RF_50R` com stackup/impedância fornecidos pela fabricante escolhida; depois revisar o trecho módulo–ESD–U.FL sem vias.
4. Confirmar a placa filha do BH1750, comprimento do cabo, domo/difusor e vedação mecânica. Implementar I²C3 e DHT22 em PA9 somente na branch de firmware planejada.
5. Regenerar BOM/netlist, executar ERC e DRC finais, revisar 3D e gerar Gerbers. A existência de conexões não roteadas impede qualquer liberação atual.
6. Ensaiar a fonte 12 V → 3,3 V, o comportamento térmico no abrigo, sensores de vento com cabos reais e leitura de iluminância em campo.
