# Decisão de engenharia — BH1750 em placa filha e liberação de I²C3

**Data:** 27/09/2026  
**Status:** decisão de arquitetura aprovada; alterações no KiCad e firmware ainda pendentes.  
**Escopo:** placa principal da estação meteorológica, placa filha do sensor de iluminância BH1750 e firmware STM32F446RET6.

## 1. Contexto

O BH1750 existente na PCB principal seria instalado dentro do abrigo Stevenson. Nessa posição ele não recebe a iluminação ambiente de forma representativa e, portanto, não cumpre adequadamente a função de medir iluminância incidente.

A ideia inicial de usar a leitura em lux para estimar o ponto de máxima potência (MPPT) do painel solar foi descartada. Lux não equivale a irradiância em W/m² e não determina o ponto de máxima potência de um painel, que depende também de espectro solar, temperatura da célula, ângulo de incidência e curva I–V. O carregador solar externo deve definir sua tensão de MPPT a partir da especificação do painel; o BH1750 não participa desse controle.

O sensor será mantido como instrumento meteorológico auxiliar de **iluminância ambiente estimada**, desde que seja instalado externamente, com janela/domo óptico e calibração compatível com a aplicação. Ele não substitui um piranômetro quando for necessária medição de irradiância solar.

## 2. Decisão adotada

1. O BH1750 será removido da PCB principal e montado em uma **placa filha externa**.
2. A placa filha será ligada à PCB principal por um conector JST e cabo curto.
3. O BH1750 usará o periférico **I²C3**, independente do I²C1 já utilizado pelo BMP280.
4. O I²C3 será mapeado em `PA8 = I2C3_SCL` e `PC9 = I2C3_SDA`.
5. O sinal do DHT22 será realocado de `PA8` para `PA9`, liberando PA8 para o I²C3.
6. A placa filha será um projeto KiCad próprio, no mesmo repositório e compartilhando as bibliotecas locais necessárias.

## 3. Mapa de pinos do STM32F446RET6

O encapsulamento usado é LQFP-64. A situação foi conferida com o esquemático ativo e com o datasheet STM32F446xC/E.

| Periférico | Sinais e pinos possíveis | Estado no projeto | Decisão |
| --- | --- | --- | --- |
| I²C1 | `PB6` SCL / `PB7` SDA | Em uso pelo BMP280 e BH1750 atual | Permanece para o BMP280. |
| I²C1 | `PB8` SCL / `PB9` SDA | Livres | Não usar: seriam o mesmo periférico I²C1, não um barramento independente. |
| I²C2 | `PB10` SCL / `PB3` SDA | PB10 livre; PB3 em uso como SWO | Não usar: exigiria perder SWO. |
| I²C3 | `PA8` SCL / `PC9` SDA | PA8 em uso pelo DHT22; PC9 livre | Par selecionado após mover o DHT22. |
| I²C3 | `PA8` SCL / `PB4` SDA | PA8 em uso pelo DHT22; PB4 livre | Alternativa não escolhida. |

`PB11` não está disponível no encapsulamento LQFP-64 do STM32F446RET6. Portanto, não forma um segundo par I²C2 utilizável nesta placa.

### Mapeamento final previsto

```text
DHT22_DATA      PA9   (pino físico 42)
BH_I2C3_SCL     PA8   (pino físico 41, AF4)
BH_I2C3_SDA     PC9   (pino físico 40, AF4)
```

## 4. Realocação do DHT22: PA8 para PA9

A mudança é eletricamente segura. PA8 e PA9 são GPIOs do mesmo banco (`GPIOA`), ambos adequados à lógica de 3,3 V e ao protocolo single-wire bidirecional do DHT22/AM2302. PA9 está livre no esquemático e no firmware atual; a UART em uso é a UART2 em PA2/PA3, não a USART1 de PA9.

O circuito do DHT22 não deve ser alterado:

- manter `U231` alimentado em `+3V3`;
- manter `R231 = 10 kΩ` como pull-up externo de `DHT22_DATA`;
- manter `C231 = 100 nF` de desacoplamento junto ao sensor;
- não conectar simultaneamente `DHT22_DATA` a PA8 e PA9.

O DHT22 não armazena leituras históricas. Assim, a troca de pino não afeta dados já gravados no cartão SD; o único risco é haver amostras ausentes enquanto uma versão de hardware ou firmware incompatível estiver sendo testada.

### Alterações no KiCad

1. Na folha `sheets/mcu.kicad_sch`, remover a ligação de `DHT22_DATA` ao pino `PA8` do `U101`.
2. Conectar a mesma net `DHT22_DATA` ao pino `PA9` do `U101`.
3. Não alterar a folha `sheets/dht22.kicad_sch`.
4. Executar **Update PCB from Schematic** e rotear a nova ligação entre o pad 42 do MCU e a rede do DHT22.
5. Conferir DRC, continuidade e ausência de curto entre PA8 e PA9.

### Alterações no firmware

No STM32CubeMX, abrir `firmware/stm32/Firmware_V0.8.ioc` e:

1. liberar PA8 da função `DHT22`;
2. configurar PA9 como `GPIO_Output`, rótulo `DHT22`, estado inicial alto, sem pull interno e velocidade alta;
3. regenerar o código, preservando os blocos `USER CODE`;
4. conferir em `Core/Inc/main.h` que as definições são:

```c
#define DHT22_Pin GPIO_PIN_9
#define DHT22_GPIO_Port GPIOA
```

O driver `Core/Src/dht22.c` recebe porta e pino pelo handle criado em `main.c`; não há referência fixa a PA8 no driver. A rotina de leitura e seus tempos não devem ser modificados nesta etapa.

## 5. Circuito da placa filha BH1750

O núcleo funcional atual do BH1750 deve migrar integralmente para a placa filha:

| Item atual | Função | Decisão |
| --- | --- | --- |
| `U221` BH1750FVI-TR | Sensor de iluminância | Migrar para a placa filha. |
| `R221` 10 kΩ | Pull-down de `ADDR`, endereço I²C `0x23` | Manter na placa filha. |
| `R222` 1 kΩ + `C221` 1 µF | RC de partida do pino `DVI` | Manter na placa filha. |
| `C222` 100 nF | Desacoplamento local do BH1750 | Manter junto ao CI na placa filha. |

Acrescentar na placa filha um capacitor de reserva de **1 µF a 4,7 µF** entre `+3V3` e GND, próximo ao conector ou ao BH1750. `C221` não substitui esse capacitor, pois pertence ao pino DVI.

Não é necessário conversor de nível lógico: o STM32 e o BH1750 usam 3,3 V.

## 6. Interface entre as placas

Usar em cada PCB um conector JST XH travável de quatro vias, com passo de 2,50 mm. Na PCB principal o header selecionado é o **JST B4B-XH-A(LF)(SN)**, vertical THT, LCSC **C144395**, com footprint `Connector_JST:JST_XH_B4B-XH-A_1x04_P2.50mm_Vertical` e modelo 3D padrão do KiCad. O conector correspondente da placa filha e o cabo devem preservar a mesma numeração de pinos.

| Pino | Sinal | Observação |
| --- | --- | --- |
| 1 | GND | Referência comum obrigatória. |
| 2 | `+3V3` | Alimentação da placa filha. |
| 3 | `BH_I2C3_SCL` | PA8 / I²C3 SCL. |
| 4 | `BH_I2C3_SDA` | PC9 / I²C3 SDA. |

Na PCB principal:

- os dois pull-ups de I²C para `+3V3` são **4,7 kΩ** (um em SCL e um em SDA), referências `R105` e `R106`, e devem ser posicionados fisicamente próximos ao STM32;
- a proteção ESD é feita por `D221` e `D222`, Nexperia `PESD5V0R1BSFYL`, posicionados junto ao JST no lado da entrada do cabo;
- não usar resistores série nesta primeira versão. Para I²C a 100 kHz e cabo de até aproximadamente 30 cm, a combinação de pull-ups de 4,7 kΩ e ESD de baixa capacitância é suficiente. Resistores série só devem ser reconsiderados após ensaio que comprove ringing/EMI; nunca devem ser marcados DNP se forem o único caminho em série do sinal.

Deve existir **somente um conjunto efetivo de pull-ups** no novo barramento. Não duplicar pull-ups de 4,7 kΩ na principal e na filha sem recalcular a resistência equivalente.

Para I²C direto, operar inicialmente a **100 kHz** e limitar o cabo a aproximadamente **30 cm**. Se o cabo for maior, sair do abrigo por trecho longo ou passar junto a cabos de painel solar, rádio ou potência, substituir a interface por I²C diferencial (por exemplo, PCA9615 nas duas extremidades) e cabo com pares adequados.

## 7. Organização dos projetos KiCad

As duas placas não devem compartilhar o mesmo arquivo `.kicad_pcb`, pois isso misturaria DRC, BOM, montagem e arquivos de fabricação. A solução é manter um único repositório, porém com um projeto KiCad independente para a placa filha.

```text
hardware/kicad/
├── estacao-meteorologica/
│   ├── estacao-meteorologica.kicad_pro
│   ├── estacao-meteorologica.kicad_sch
│   └── estacao-meteorologica.kicad_pcb
└── bh1750-filho/
    ├── bh1750-filho.kicad_pro
    ├── bh1750-filho.kicad_sch
    └── bh1750-filho.kicad_pcb
```

Na PCB principal permanecerão o conector JST, os pull-ups, a proteção ESD e as redes do I²C3. A placa filha conterá o conector correspondente, o BH1750 e todos os seus componentes locais. As duas podem usar as bibliotecas locais existentes, configuradas com caminhos relativos em seus arquivos de tabela de bibliotecas.

## 8. Posicionamento e óptica da placa filha

- Instalar o BH1750 fora do abrigo Stevenson, orientado para a região do céu a ser medida e sem sombra criada pela estrutura, painel ou antena.
- Usar caixa opaca e resistente a UV, com abertura restrita à área óptica do sensor.
- A solução de referência é domo externo transparente em policarbonato/acrílico resistente a UV e difusor interno de PTFE; o conjunto deve ser validado e calibrado contra um luxímetro de referência.
- Não usar a leitura em lux como variável de controle do MPPT. Registrar a grandeza como iluminância em lux, com as limitações de resposta espectral e angular documentadas.

## 9. Ordem de execução e validação

Para evitar combinar duas mudanças sem diagnóstico, executar nesta ordem:

1. Alterar somente o DHT22 de PA8 para PA9 no esquema, PCB e firmware.
2. Validar o DHT22 antes de usar PA8 para qualquer outra função.
3. Criar o projeto da placa filha e migrar o circuito do BH1750.
4. Adicionar I²C3, conector, pull-ups e proteção à PCB principal.
5. Conectar as duas placas e validar I²C3 a 100 kHz.
6. Validar a medição óptica com a montagem mecânica final.

### Critérios mínimos de aceitação

**DHT22**

- 20 leituras consecutivas com estado `DHT22_OK`;
- zero erros de timeout ou checksum;
- temperatura e umidade plausíveis; comparar com instrumento de referência quando disponível;
- repetir o ensaio após ciclo completo de alimentação.

**BH1750 / I²C3**

- detecção confiável do endereço `0x23` no I²C3;
- 20 leituras consecutivas sem erro de comunicação;
- inspeção de SCL/SDA por osciloscópio ou analisador lógico se houver falhas;
- leitura coerente em escuro, luz ambiente e luz intensa, comparada a luxímetro quando possível;
- teste com o comprimento final de cabo antes da fabricação do lote.

## 10. Arquivos afetados na futura implementação

| Área | Arquivo ou diretório | Alteração prevista |
| --- | --- | --- |
| Esquemático principal | `hardware/kicad/estacao-meteorologica/sheets/mcu.kicad_sch` | DHT22 PA8→PA9; I²C3 PA8/PC9; conector e proteção. |
| Esquemático BH atual | `hardware/kicad/estacao-meteorologica/sheets/bh1750.kicad_sch` | Migrar o circuito para o novo projeto da filha e remover da BOM/PCB principal. |
| PCB principal | `hardware/kicad/estacao-meteorologica/estacao-meteorologica.kicad_pcb` | Novo roteamento DHT22 e interface JST/I²C3. |
| Projeto da filha | `hardware/kicad/bh1750-filho/` | Criar esquemático, PCB, BOM e arquivos de fabricação independentes. |
| CubeMX | `firmware/stm32/Firmware_V0.8.ioc` | DHT22 em PA9; habilitar I²C3 em PA8/PC9. |
| Firmware gerado | `firmware/stm32/Core/Inc/main.h`, `Core/Src/main.c` | Refletir o novo pino DHT22 e a instância I²C3. |
| Driver DHT22 | `firmware/stm32/Core/Src/dht22.c` | Nenhuma alteração funcional prevista. |
| Driver BH1750 | `firmware/stm32/Core/Src/bh1750.c` | Migrar o handle do I²C1 para I²C3 durante a integração. |

## 11. Referências

- STM32F446xC/E datasheet, tabela de pinagem e funções alternativas: `docs/engineering/hardware/datasheets/STM32F446xCE.pdf`.
- Esquemático do MCU: `hardware/kicad/estacao-meteorologica/sheets/mcu.kicad_sch`.
- Esquemático do DHT22: `hardware/kicad/estacao-meteorologica/sheets/dht22.kicad_sch`.
- Esquemático atual do BH1750: `hardware/kicad/estacao-meteorologica/sheets/bh1750.kicad_sch`.
- BH1750FVI-TR datasheet: `https://fscdn.rohm.com/en/products/databook/datasheet/ic/sensor/light/bh1750fvi-e.pdf`.
