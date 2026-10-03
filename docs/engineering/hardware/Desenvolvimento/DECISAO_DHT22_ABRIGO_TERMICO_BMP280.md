# Decisão de engenharia — DHT22 no abrigo, influência térmica da PCB e uso do BMP280

**Data:** 27/09/2026  
**Status:** diretriz de posicionamento aprovada; alteração mecânica/PCB ainda pendente.  
**Escopo:** medição de temperatura e umidade da estação meteorológica dentro de abrigo passivo tipo Stevenson, com a PCB principal instalada no centro do abrigo.

## 1. Contexto mecânico

A estação usará um abrigo de radiação passivo semelhante a um abrigo Stevenson de discos: possui aberturas em toda a volta e reduz a incidência direta do Sol sobre os sensores. A PCB principal ficará encaixada na região central do abrigo e ocupará praticamente todo o seu volume disponível.

Nessa geometria, não há espaço adequado para instalar uma placa filha dedicada ao DHT22 fora da PCB principal. Portanto, o DHT22 permanecerá eletricamente ligado à PCB principal; o objetivo do layout será reduzir, e não presumir eliminar, a influência térmica da placa.

## 2. Efeito térmico esperado

As aberturas do abrigo promovem ventilação natural e reduzem o aquecimento por radiação solar, mas não isolam completamente o sensor das fontes internas de calor. O erro será mais provável em condição de sol forte e pouco vento, quando a renovação de ar é menor.

O DHT22 pode ser influenciado por três mecanismos:

1. **Condução:** calor proveniente da PCB pode chegar ao sensor pelos seus terminais, cobre e material FR-4.
2. **Convecção:** o ar aquecido por reguladores, MCU, rádio e pela própria PCB tende a circular pelo volume interno do abrigo.
3. **Radiação:** a PCB pode absorver calor e irradiá-lo para o encapsulamento do sensor, mesmo sem iluminação solar direta sobre ele.

Por isso, a presença de ventilação passiva não torna equivalente posicionar o DHT22 em qualquer ponto da PCB. O ponto de montagem continua determinante para a qualidade de temperatura e umidade relativa medidas.

## 3. Decisão de posicionamento do DHT22

O DHT22 deverá permanecer na PCB principal, porém deve ser reposicionado para a **borda da placa mais próxima de uma abertura lateral do abrigo** e o mais distante possível das fontes de calor.

### Regras de layout e mecânica

- Posicionar `U231` no perímetro da PCB, no lado oposto aos conversores de tensão, indutores, MCU e módulo LoRa.
- Orientar a face perfurada do DHT22 para o fluxo de ar proveniente das aberturas laterais do abrigo; não deixá-la bloqueada pela face da PCB, pela bateria ou por conectores.
- Sempre que a mecânica permitir, deixar o encapsulamento avançar além do contorno da PCB, mantendo apenas os terminais sobre a placa.
- Criar um recorte ou uma pequena lingueta/ilha térmica para o sensor. O recorte interrompe a continuidade de FR-4 e cobre entre o corpo do sensor e o restante da placa.
- Remover plano de cobre e evitar trilhas de potência sob e nas proximidades imediatas do DHT22. Manter somente as trilhas estritamente necessárias para alimentação e `DHT22_DATA`.
- Não instalar o DHT22 acima de fontes quentes no sentido vertical do abrigo, pois o ar aquecido sobe. Se a orientação de montagem for vertical, privilegiar o ponto lateral/baixo em relação a reguladores e rádio.
- Manter o circuito elétrico já definido: `R231 = 10 kΩ` como pull-up de dados e `C231 = 100 nF` como desacoplamento local.

### Alternativa permitida sem placa filha

Se houver folga mecânica mínima, o DHT22 poderá ser ligado à mesma PCB por três fios curtos, de aproximadamente **2 a 5 cm**, ou montado em uma pequena lingueta da própria PCB. Isso não cria uma segunda placa e diminui o acoplamento térmico direto entre o encapsulamento e a área principal da PCB.

Esta alternativa deve respeitar o roteamento e a vedação do abrigo; os fios não devem obstruir as aberturas nem levar o sensor a uma posição exposta à água direta.

## 4. O que não será feito: correção automática com a temperatura do BMP280

O BMP280 está mais próximo da região de conversores de tensão. Sua leitura de temperatura representa principalmente a temperatura local do encapsulamento e da PCB naquela região; ela é necessária para a compensação interna de pressão do próprio BMP280, mas **não é uma referência válida de temperatura ambiente**.

Não será implementado um offset automático do tipo:

```text
temperatura_publicada = temperatura_DHT22 + f(temperatura_BMP280)
```

Essa abordagem foi rejeitada porque a diferença entre os dois sensores não é constante. Ela varia, entre outros fatores, com:

- corrente e perdas dos conversores;
- atividade e transmissão do LoRa;
- tensão e condição da bateria;
- incidência solar sobre o abrigo e a PCB;
- velocidade/direção do vento e renovação interna do ar;
- diferenças de tempo de resposta e erro individual dos sensores.

Um offset variável baseado no BMP280 poderia corrigir artificialmente uma leitura em uma condição e piorá-la em outra. Além disso, alterar somente a temperatura sem um modelo psicrométrico coerente pode tornar a umidade relativa calculada pelo DHT22 inconsistente.

### Uso correto da temperatura do BMP280

A temperatura do BMP280 será usada para:

1. compensação interna da leitura de pressão, conforme o driver/datasheet do BMP280;
2. diagnóstico de aquecimento local: uma diferença persistente e elevada entre BMP280 e DHT22 indica que a PCB ou a região do BMP está aquecendo;
3. avaliação de posicionamento durante testes térmicos.

Ela não será usada como correção meteorológica automática da temperatura do DHT22.

## 5. Calibração correta da temperatura e umidade ambiente

Depois de definido o abrigo e o posicionamento físico final, a temperatura e a umidade do DHT22 devem ser comparadas a um instrumento de referência instalado em condição equivalente de abrigo e ventilação.

O ensaio deve incluir, no mínimo:

| Condição | Objetivo |
| --- | --- |
| Noite ou céu encoberto | Obter condição com baixa carga solar e verificar offset básico. |
| Sol com vento | Avaliar a ventilação natural do abrigo. |
| Sol com pouco vento | Verificar o pior caso de aquecimento interno e por radiação. |
| Operação com transmissão LoRa e conversores ativos | Identificar influência elétrica/térmica da PCB. |

Caso a comparação revele erro repetível e estável, poderá ser aplicado no firmware um ajuste documentado, derivado do instrumento de referência. Esse ajuste deve ser uma decisão de calibração do conjunto mecânico final, e não uma função instantânea da temperatura do BMP280.

## 6. Requisitos para o BMP280

O BMP280 pode permanecer na PCB principal para medição de pressão, desde que:

- fique afastado o máximo possível dos conversores, indutores e fontes de calor disponíveis;
- tenha comunicação com o ar ambiente por abertura/ventilação protegida, sem bloqueio pela placa;
- seja protegido de entrada direta de água e de luz solar direta;
- não seja tratado como sensor de temperatura ambiente de referência.

Uma membrana hidrofóbica e permeável ao ar (por exemplo, ePTFE apropriada para ventilação) pode ser considerada na abertura de pressão se houver risco de respingos ou condensação. A escolha deve ser validada para não bloquear a equalização de pressão.

## 7. Relação com as mudanças de I²C3 e BH1750

Esta decisão não altera o circuito do DHT22 nem invalida a migração planejada do seu pino de dados de `PA8` para `PA9`. A mudança de pino continua necessária para liberar `PA8` como `I2C3_SCL` da placa filha externa do BH1750.

As ações são complementares:

| Item | Decisão |
| --- | --- |
| DHT22 | Permanece na PCB principal, mas em borda ventilada e termicamente isolada por layout. |
| DHT22_DATA | Migra de `PA8` para `PA9`. |
| BMP280 | Permanece na PCB principal para pressão; sua temperatura é diagnóstica/compensatória. |
| BH1750 | Migra para placa filha externa, ligada por I²C3 em `PA8`/`PC9`. |

O documento completo da mudança de pinos e do BH1750 está em `DECISAO_BH1750_PLACA_FILHA_I2C3_DHT22.md`.

## 8. Sequência de implementação

1. Definir o lado da PCB que ficará voltado às aberturas mais livres do abrigo e confirmar a orientação vertical/horizontal de montagem.
2. Reposicionar `U231` para essa borda, distante das fontes de calor.
3. Criar o recorte/ilha térmica e a área sem cobre no layout da PCB.
4. Conferir se corpo, terminais e aberturas do DHT22 não colidem com o abrigo nem bloqueiam a circulação de ar.
5. Implementar a mudança elétrica `DHT22_DATA: PA8 → PA9` conforme o documento do BH1750.
6. Executar DRC e validar eletricamente o DHT22 com leituras consecutivas sem timeout ou erro de checksum.
7. Realizar teste comparativo térmico e de umidade no abrigo final antes de congelar o layout para fabricação.

## 9. Critérios mínimos de aceitação

- O sensor recebe ar pelas aberturas laterais sem obstrução física da PCB.
- Não há cobre de plano, trilha de potência, regulador, indutor ou módulo LoRa imediatamente sob ou adjacente ao encapsulamento.
- O DHT22 permanece operacional após a mudança para PA9: pelo menos 20 leituras consecutivas válidas, sem timeout ou erro de checksum.
- A diferença frente ao instrumento de referência é registrada nas condições de teste definidas; o comportamento em sol e pouco vento deve ser conhecido antes de declarar a leitura como temperatura ambiente representativa.
- Não há correção automática baseada na temperatura do BMP280.

## 10. Arquivos que poderão ser afetados na implementação

| Área | Arquivo | Alteração prevista |
| --- | --- | --- |
| Esquemático DHT22 | `hardware/kicad/estacao-meteorologica/sheets/dht22.kicad_sch` | Nenhuma alteração funcional esperada; manter R231 e C231. |
| Esquemático MCU | `hardware/kicad/estacao-meteorologica/sheets/mcu.kicad_sch` | Mover `DHT22_DATA` de PA8 para PA9. |
| PCB principal | `hardware/kicad/estacao-meteorologica/estacao-meteorologica.kicad_pcb` | Reposicionar U231, criar isolamento térmico/recorte e rotear a nova net. |
| CubeMX | `firmware/stm32/Firmware_V0.8.ioc` | Configurar PA9 como GPIO do DHT22 e liberar PA8 para I²C3. |
| Firmware gerado | `firmware/stm32/Core/Inc/main.h`, `firmware/stm32/Core/Src/main.c` | Refletir `DHT22_Pin = GPIO_PIN_9`. |

## 11. Referências

- Documento da decisão de BH1750/I²C3/DHT22: `docs/engineering/hardware/Desenvolvimento/DECISAO_BH1750_PLACA_FILHA_I2C3_DHT22.md`.
- Bosch Sensortec, BMP280 Digital Pressure Sensor: https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bmp280-ds001.pdf.
- World Meteorological Organization, *Guide to Instruments and Methods of Observation* (WMO-No. 8): https://urban-climate.org/wp-content/uploads/2023/10/WMO_2023_1292_en.pdf.
