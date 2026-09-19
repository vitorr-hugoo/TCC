# Resultado da validação — PWR-1

Verificação final desta etapa: 19/09/2026, KiCad 9.0.6. Os arquivos de referência são `netlist.xml`, `erc_depois.json` e `resumo_validacao.json` nesta pasta.

## Resultado

| Verificação | Resultado |
|---|---|
| ERC da folha `/alimentacao/` | **0 erros e 0 avisos** |
| Novas ocorrências ERC no projeto, por comparação com a linha de base | **0** |
| ERC completo antes | 71 erros + 27 avisos = 98 |
| ERC completo depois | 49 erros + 27 avisos = 76 |
| Contratos de conexão, BOM, footprints e integração | **146 aprovados, 2 reprovados** |
| Arquivo `.kicad_pcb` | Não alterado; não há layout desta implementação |

## Pendência de integração — não liberar fabricação

Os dois testes reprovados descrevem **um único problema preexistente**: o +5V externo está ligado a U5/CN7_18, em vez de U5/CN7_6 (E5V). O primeiro teste exige alimentação em E5V; o segundo exige isolamento da saída CN7_18.

Foi solicitada autorização para a alteração mínima na folha MCU, porque o escopo original restringia as mudanças à alimentação e sua declaração na raiz. Enquanto essa autorização não for respondida, o fio permanece inalterado. Configurar somente JP5 não corrige o pino de entrada usado no esquema.

Com autorização, a correção proposta é mover o fio/rótulo +5V de CN7_18 para CN7_6, mover o marcador de não conexão para CN7_18 e inserir uma nota sobre JP5/E5V, JP1 e ordem de conexão do USB. Depois disso, reexportar netlist/ERC e executar novamente `validar_alimentacao.py`.

Referência: [ST UM1724, seções 7.5.2–7.5.4](https://www.st.com/resource/en/user_manual/um1724-stm32-nucleo64-boards-mb1136-stmicroelectronics.pdf).

## O que foi conferido

- Entradas J60/J61 separadas com JP60 aberto; pino comum indo a F60.
- TVS pós-fusível, PMOS com os dois drenos ligados, source para carga, zener e resistor de gate.
- Redes VIN, SW, BST, FB e EN dos dois bucks, além dos retornos e capacitores.
- Saídas +5V/+3V3 distintas; remoção autorizada de U10 do BH1750, sem paralelismo de reguladores.
- C69 DNP; 40,5 µF nominais de capacitância em +3V3 visível na netlist.
- Existência de todos os footprints da nova folha; correspondência de pinos/pads; modelos 3D dos dois importados.
- Código JLC, MPN e disponibilidade de pré-compra positiva para todos os itens de compra da nova folha.
- Ausência de novas ocorrências ERC por assinatura tipo/folha/UUID, sem criar exclusões de ERC para esconder problemas.
- Exportação SVG/PNG e revisão visual do circuito, rótulos, valores e integração mínima na folha BH1750.

As 76 ocorrências ERC remanescentes pertencem à raiz e às folhas MCU, BMP280, BH1750, cartão SD, LoRa e DHT22. Não foram corrigidas fora do escopo. O arquivo JSON contém suas descrições e identificadores.

## Limites da verificação

Não foram realizadas simulação analógica, análise de malha, ensaios elétricos, térmicos, de surto ou EMI, nem validação mecânica em bancada dos conectores. A auditoria de números de pads não substitui comparação dimensional com desenho do fabricante. Nenhum componente foi comprado ou reservado. Estoque do catálogo público não equivale a reserva interna na linha PCBA.

O script retorna código 1 enquanto houver testes reprovados, mesmo que o ERC da folha esteja limpo. Ele não altera o projeto; `--emit-docs-patch` apenas imprime um patch dos relatórios.
