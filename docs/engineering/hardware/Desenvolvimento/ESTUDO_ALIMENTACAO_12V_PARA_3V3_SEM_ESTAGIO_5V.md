# Estudo de decisão — alimentação direta de `VIN_MAIN` para `+3V3`

**Data do estudo:** 27/09/2026  
**Estado em 02/10/2026:** decisão **implementada no esquemático**. Este estudo registra a análise que levou à decisão; o estado de implementação, BOM e pendências está em [DECISOES_IMPLEMENTACAO_PCB_2026-10.md](DECISOES_IMPLEMENTACAO_PCB_2026-10.md).

> **Atualização de resultado:** foi adotado `VIN_MAIN → U601 AP63203WU-7 → +3V3`, com `L601` ANR5040T3R9M de 3,9 µH. `U602`, `L602`, `R602`, `R603` e a rede funcional `+5V` foram removidos da folha ativa. Assim, trechos abaixo que dizem “se a decisão for aprovada” devem ser lidos como histórico da alternativa que já foi executada, não como pendência de escolha.

## Conclusão executiva

É tecnicamente viável remover a conversão intermediária para `+5V`, pois a verificação do esquemático atual não encontrou nenhuma carga externa alimentada por essa rede. Porém, **não é viável ligar os 12 V diretamente ao conversor atual de 3,3 V**: o `U602` (`TLV62569DBVR`) tem faixa de entrada de somente **2,5 a 5,5 V**. Aplicar `VIN_MAIN` diretamente nele pode danificá-lo.

A opção recomendada é trocar o buck fixo de 5 V `U601` pelo membro fixo de 3,3 V da mesma família, o **AP63203WU-7** (LCSC/JLCPCB `C780769`), e eliminar todo o segundo buck `U602`. O novo conversor opera de 3,8 a 32 V, entrega 3,3 V fixos e até 2 A. Ainda assim, a alteração requer revisão do indutor, do layout e ensaios elétricos; não é apenas uma troca de etiqueta de rede.

> **Recomendação preliminar:** usar um único buck de entrada ampla `VIN_MAIN → +3V3`, com o AP63203 ou outro buck equivalente de entrada ampla. Não usar LDO como regulador principal da placa.

## 1. Estado confirmado no projeto atual

Na folha [`alimentacao.kicad_sch`](../../../../hardware/kicad/estacao-meteorologica/sheets/alimentacao.kicad_sch), a topologia implementada é:

```text
VIN_MAIN
  │
  ├─ proteção de entrada: seleção / fusível / TVS / PMOS / capacitores de entrada
  │
  └─ U601 AP63205WU-7 (buck fixo de 5 V)
       └─ +5V
            └─ U602 TLV62569DBVR (buck ajustável de 3,318 V)
                 └─ +3V3
```

| Item | Implementação atual | Constatação para a decisão |
| --- | --- | --- |
| Primeiro buck | `U601`, AP63205WU-7, 3,8–32 V de entrada, saída fixa de 5 V | É o único estágio que hoje pode receber `VIN_MAIN`. |
| Segundo buck | `U602`, TLV62569DBVR, entrada de 2,5–5,5 V | **Não suporta 12 V.** Não pode ser reaproveitado diretamente depois de remover o 5 V. |
| Ajuste do U602 | `R602 = 453 kΩ` e `R603 = 100 kΩ` | Programam aproximadamente 3,318 V e deixam de ser necessários com uma saída fixa de 3,3 V. |
| Indutor do U602 | `L602`, 2,2 µH, Abracon ASPI-0530HI-2R2M-T2 (`C7217567`) | Deixa de ser necessário. |
| Capacitores `C609–C611` | 3 × 10 µF / 6,3 V, ligados a `+3V3` | São capacitores de saída locais do trilho de 3,3 V; **não devem ser removidos automaticamente**. Precisam ser reavaliados na compensação/capacitância efetiva do novo buck. |
| Rede `+5V` | `U601`, `L601`, `C606–C608`, `TP602` e entrada/EN de `U602` | Não há sensor, MCU, LoRa, microSD ou conector de usuário alimentado por `+5V` no esquemático atual. |

Os capacitores de entrada de alta tensão (`C601–C605`), o fusível, o TVS e o PMOS de proteção **continuam necessários**. Remover o estágio de 5 V não torna a entrada de 12 V inerentemente segura.

## 2. Limite que impede reutilizar o U602

O fabricante especifica o TLV62569 para alimentação de 2,5 a 5,5 V. Ele é excelente como buck de baixa tensão, mas não é um regulador de 12 V. Logo, as alternativas seguras são:

1. manter os dois estágios atuais;
2. retirar U602 e configurar outro conversor de entrada ampla para 3,3 V; ou
3. substituir U602 por outro buck que aceite todo o envelope de `VIN_MAIN`.

Ligar `VIN_MAIN` ao pino `VIN` do U602 não é uma alternativa de teste aceitável. O datasheet do [TLV62569](https://www.ti.com/lit/ds/symlink/tlv62569.pdf) define o máximo operacional de 5,5 V.

## 3. Alternativas analisadas

### Alternativa A — um único buck fixo de 3,3 V (recomendada)

Substituir `U601 AP63205WU-7` pelo **AP63203WU-7** e fazer a saída dele ser diretamente a rede `+3V3`:

```text
VIN_MAIN → proteção existente → AP63203WU-7 → +3V3
```

O AP63203 pertence à mesma família AP6320x do AP63205. Segundo o [datasheet da família](https://www.diodes.com/datasheet/download/AP63200-AP63201-AP63203-AP63205.pdf), ele é um buck síncrono fixo de 3,3 V, 2 A, com entrada de 3,8–32 V. A página do [componente na JLCPCB](https://jlcpcb.com/partdetail/DiodesIncorporated-AP63203WU7/C780769) confirma o encapsulamento TSOT-23-6 e o código `C780769`.

**Vantagens**

- elimina U602, L602, R602 e R603;
- elimina o nó de comutação do segundo buck e uma fonte adicional de ruído;
- reduz a cascata de perdas de conversão;
- libera área, principalmente a ocupada por `L602` (corpo de aproximadamente 5,7 × 5,3 mm), e simplifica o roteamento da região de potência;
- preserva um regulador de comutação, portanto mantém eficiência adequada à LoRa, microSD e picos do MCU.

**Mudanças obrigatórias no esquema/BOM**

| Ação | Detalhe |
| --- | --- |
| Substituir U601 | `AP63205WU-7` → `AP63203WU-7`, atualizar MPN, LCSC (`C780769`), ficha e símbolo se necessário. Não assumir pinagem sem conferir a tabela de pinos do datasheet durante a edição. |
| Alterar a rede de saída do primeiro buck | `+5V` passa a ser `+3V3`, incluindo rótulos, ponto de teste e hierarquia. Não deve existir uma união acidental entre a antiga saída de 5 V e a nova saída de 3,3 V. |
| Alterar L601 | Para o AP63203, a aplicação recomendada do fabricante usa **3,9 µH**, enquanto o AP63205 de 5 V usa 4,7 µH. Escolher um indutor de 3,9 µH com corrente de saturação/RMS, DCR e footprint adequados; não manter 4,7 µH por conveniência sem recalcular/validar ripple e estabilidade. |
| Remover o segundo estágio | Remover `U602`, `L602`, `R602`, `R603` e `TP602` (`+5V`). A rede `+5V` deve desaparecer do projeto, salvo uma futura carga real de 5 V. |
| Preservar proteção e capacitores relevantes | Manter a proteção de entrada e os capacitores `C601–C605`. `C606`, `C607` e `C608`, hoje associados ao estágio U601, devem ser reavaliados e em princípio reaproveitados como capacitores de saída/boot do AP63203 conforme o datasheet. `C609–C611` continuam como reserva local de `+3V3`, sujeitos à validação abaixo. |
| Refazer o layout do buck | O loop `VIN–CIN–U601–GND`, o nó `SW`, o indutor, os capacitores de saída e o retorno de FB devem seguir o layout recomendado para a nova configuração. Não reutilizar trilhas do segundo estágio. |

O datasheet recomenda, para o AP63203 em 3,3 V, indutor de 3,9 µH, capacitor de entrada de 10 µF, dois capacitores de saída de 22 µF e capacitor bootstrap de 100 nF. Esses valores são a referência de partida; a capacitância efetiva dos MLCC sob polarização DC e a capacitância já distribuída pela placa devem ser verificadas antes de congelar a BOM.

### Alternativa B — um buck de entrada ampla diferente

Também é possível substituir a cascata por outro conversor `12 V → 3,3 V` que aceite o envelope completo de entrada. Por exemplo, o [LMR23610](https://www.ti.com/product/LMR23610) aceita 4–36 V e entrega até 1 A; há referência do fabricante para 12 V → 3,3 V/1 A. É uma opção se medições mostrarem que 1 A atende a carga e se custo, disponibilidade, encapsulamento e layout forem melhores no momento da compra.

**Quando escolher esta alternativa:** se for desejado um controlador com recursos específicos, outra frequência de comutação, disponibilidade superior ou solução de EMI/temperatura comprovadamente melhor. Ela exige projeto de indutor, capacitores e layout próprios; não é um substituto direto de U601.

### Alternativa C — LDO direto de 12 V para 3,3 V (não recomendada)

Para o trilho principal da estação, um LDO transforma quase toda a diferença de tensão em calor:

```text
P_perdida = (VIN − 3,3 V) × I_3V3
```

| Corrente em `+3V3` | Perda no LDO a 12 V | Perda no LDO a 16 V |
| ---: | ---: | ---: |
| 100 mA | 0,87 W | 1,27 W |
| 200 mA | 1,74 W | 2,54 W |
| 500 mA | 4,35 W | 6,35 W |
| 1,0 A | 8,70 W | 12,70 W |

Essas perdas são incompatíveis com o encapsulamento/área disponível e aqueceriam justamente o abrigo meteorológico. Além de prejudicar a autonomia da bateria, aumentariam a influência térmica na medição de temperatura e umidade. Um LDO pode fazer sentido apenas como pós-regulador muito local e de baixa corrente para uma referência analógica extremamente sensível — não como a fonte principal de `+3V3`.

O LDO que já apareceu como referência histórica (`MCP1700`) também não serve como alternativa direta de 12 V para o trilho principal: sua corrente nominal é baixa e sua tensão de entrada máxima deve sempre ser conferida no datasheet da variante antes de qualquer uso. Ele não resolve os picos de rádio/cartão nem o problema térmico.

### Alternativa D — manter a cascata 12 V → 5 V → 3,3 V

É a opção de menor alteração, pois já está no esquemático. Ela só se justifica se a placa precisar de uma carga real de 5 V no curto prazo, ou se houver uma razão validada de arquitetura para separar os dois estágios. Sem carga de 5 V, ela acrescenta componentes, área, perdas e outro nó de comutação sem benefício funcional atual.

## 4. Eficiência e impacto energético

Em uma cascata, a eficiência total é aproximadamente o produto das eficiências de cada estágio:

```text
η_total = η_12→5 × η_5→3V3
```

Como ilustração, se cada buck tiver 90% de eficiência, a cascata terá aproximadamente 81%. Um único buck a 90% evita uma conversão. Para uma carga de 1 A em 3,3 V:

| Arquitetura ilustrativa | Potência entregue | Potência de entrada estimada | Perda estimada |
| --- | ---: | ---: | ---: |
| Dois bucks, 90% × 90% | 3,30 W | 4,07 W | 0,77 W |
| Um buck, 90% | 3,30 W | 3,67 W | 0,37 W |
| Diferença | — | — | **≈ 0,41 W a menos** |

Em 300 mA na saída, a mesma hipótese indica cerca de 0,12 W a menos. Estes valores são **estimativas de arquitetura**, não uma promessa: eficiência depende de tensão da bateria, corrente média/picos, indutor, temperatura e modo de carga leve. A confirmação deve vir de medição de corrente na bateria e temperatura dos componentes com LoRa transmitindo e cartão microSD ativo.

## 5. Economia de custo e de área

A simplificação reduz quatro posições montadas diretamente ligadas ao segundo estágio: `U602`, `L602`, `R602` e `R603`. Também remove a área e o roteamento do segundo nó SW. Os capacitores `C609–C611` são de `+3V3`; por isso, **não** foram contabilizados como economia garantida.

Os preços de componentes variam e não devem ser tomados como cotação de fabricação. Como referência:

- a BOM histórica local, consultada em 18/09/2026, registra `U601 AP63205` em US$ 0,4216 (unidade), `U602 TLV62569` em US$ 0,0795 e resistores de realimentação por poucos milésimos de dólar;
- a página da [LCSC para L602](https://www.lcsc.com/product-detail/power-inductors_abracon-aspi-0530hi-2r2m-t2_C7217567.html) mostrou US$ 0,8389/unidade, mas com estoque muito baixo na consulta indexada;
- a página da [LCSC para AP63203](https://www.lcsc.com/product-detail/DC-DC-Converters_Diodes-Incorporated-AP63203WU-7_C780769.html) mostrou US$ 1,217/unidade e estoque disponível na consulta.

Portanto, **não se deve esperar economia automática de material** apenas pela redução de componentes: o AP63203 pode custar mais que o AP63205, e será necessário trocar `L601` por 3,9 µH. Na cotação unitária de referência, a economia de `U602 + L602 + R602 + R603` fica próxima do adicional de preço do AP63203; em volume e na PCBA, a conclusão pode inverter. Os ganhos mais seguros são:

- menor número de componentes e operações de montagem;
- menos uma linha de componente especial/extendido para a montagem, sujeito às regras do fornecedor;
- região de potência menor e mais simples;
- menor perda energética, com potencial de melhorar autonomia e temperatura interna.

Antes de decidir pelo custo, gerar uma BOM temporária para as duas variantes e cotar ambas na mesma data, mesma quantidade e mesma modalidade de montagem. Isso elimina a comparação enganosa entre preços de datas diferentes, quantidades diferentes e estoque volátil.

## 6. Pontos de atenção específicos da estação

1. **Envelope de entrada, não somente “12 V”:** a bateria VRLA e a carga solar podem produzir tensão acima de 12 V; o projeto deve ser validado em toda a faixa real de `VIN_MAIN` (incluindo condição de carga, partida e transientes). A margem de 32 V do AP63203 não substitui TVS, fusível nem proteção contra inversão.
2. **Térmica e metrologia:** o buck único ainda dissipará calor. Mantê-lo na região inferior da PCB, longe de DHT22 e BMP280, como já previsto no floorplan. O ganho é remover o calor do segundo buck, não eliminar toda fonte térmica.
3. **Ruído:** haverá apenas um nó SW, mas ele agora converte diretamente 12 V para 3,3 V. O nó SW deve ser compacto, sem vias, longe de ADC, I²C, cristais, LoRa e sensores.
4. **Capacitância efetiva:** MLCC de 10 µF/6,3 V pode perder capacitância com polarização DC. Verificar a capacitância efetiva total de `C606`, `C607` e `C609–C611` e confrontar com a recomendação do AP63203; não somar apenas valores nominais.
5. **Picos reais:** o limite de 2 A do CI não dispensa medir consumo de partida, transmissão LoRa, escrita no microSD e corrente de periféricos. O limite térmico da PCB pode ser menor que o limite nominal do CI.
6. **Futuro 5 V:** se um sensor, atuador, USB host ou periférico de 5 V for adicionado depois, será necessário um novo buck de 5 V. Não converter `+3V3` para 5 V sem análise de potência.

## 7. Plano de validação antes da fabricação

1. Medir a corrente de `+3V3` em repouso, LoRa TX, escrita contínua no microSD e partida; registrar média e picos.
2. Confirmar a faixa observada de `VIN_MAIN` na bateria descarregada, nominal, em flutuação/carga solar e durante reconexão.
3. Montar/prototipar o buck único e testar em pelo menos os extremos de entrada definidos no passo anterior.
4. Medir `+3V3` com osciloscópio no capacitor de saída: partida, ripple, queda durante picos e comportamento sob carga transitória.
5. Medir temperatura de U601/AP63203 e do indutor após regime térmico, com a placa dentro do abrigo ou em condição térmica equivalente.
6. Rodar ERC e DRC depois da edição, verificar a ausência completa de `+5V` e conferir em 3D/Gerber a nova área de potência.
7. Atualizar BOM, CPL, dados LCSC, ficha técnica, testes de ponto de prova e documentação de layout antes de liberar fabricação.

## 8. Decisão proposta

| Critério | Dois bucks atuais | Buck único AP63203 | LDO direto |
| --- | --- | --- | --- |
| Compatível com `VIN_MAIN` de 12 V | Sim | Sim | Depende do LDO, mas termicamente inadequado |
| Compatível com picos de 3,3 V | Sim, a validar | Sim, a validar | Não recomendado |
| Eficiência/autonomia | Menor | Maior | Muito baixa |
| Calor dentro do abrigo | Dois estágios | Um estágio | Alto/crítico |
| Componentes/área | Maior | Menor | Menor, mas inviável como trilho principal |
| Risco de mudança | Baixo | Médio: requer redesenho/validação | Alto: risco térmico e elétrico |
| Recomendação | Somente se 5 V voltar a ser necessário | **Recomendada** | **Rejeitada** |

Se a decisão for aprovar a Alternativa A, a próxima tarefa deve ser uma alteração controlada do esquemático e da PCB — substituição de U601/L601, remoção de U602/L602/R602/R603/TP602, atualização de redes e revalidação de layout. Essa alteração deve ocorrer antes de iniciar o roteamento definitivo.
