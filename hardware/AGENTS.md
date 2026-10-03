# Hardware — PCB da estação meteorológica

## Escopo e fonte de verdade

Este arquivo rege `hardware/`, incluindo o projeto KiCad, bibliotecas e documentação de engenharia. O projeto ativo está em `hardware/kicad/estacao-meteorologica/`; seus arquivos `.kicad_sch`, `.kicad_pcb`, `.kicad_pro`, folhas hierárquicas e tabelas de bibliotecas são a fonte de verdade elétrica e mecânica.

Antes de uma alteração de hardware:

1. Execute `git status --short`.
2. Leia `docs/engineering/hardware/Desenvolvimento/README.md`.
3. Leia somente os documentos de decisão pertinentes à tarefa. Para a PCB V2, use em particular `DECISOES_IMPLEMENTACAO_PCB_2026-10.md` e `Registro_de_alteracoes_hardware.md`.
4. Diferencie decisão implementada, diretriz de layout aprovada e item pendente de validação em bancada ou firmware.

Planos, SVGs, relatórios de ERC/DRC, netlists, imagens e BOMs exportadas podem registrar revisões anteriores. Não os trate como fonte de verdade sem confrontá-los com o KiCad ativo.

## Limites de escopo

- Preserve alterações existentes; não reorganize, reimporte ou reescreva arquivos KiCad fora do pedido.
- Não altere firmware nesta área. Se a tarefa mudar pinagem ou interface, registre o impacto para `firmware/`, mas não edite seu código sem solicitação explícita.
- Antes de editar diretamente arquivos KiCad com ferramentas de texto ou scripts, confirme que o KiCad está fechado.
- Use bibliotecas locais declaradas pelo projeto. Não substitua símbolos, footprints ou modelos por itens de bibliotecas pessoais/globais sem verificar e atualizar as referências locais.
- Ao trocar um componente, confira e mantenha coerentes símbolo, pinagem, footprint, modelo 3D, MPN, LCSC, datasheet, descrição e BOM.

## Premissas consolidadas da PCB V2

- A alimentação é `VIN_MAIN → AP63203WU-7 → +3V3`; não existe barramento funcional `+5V`. Não restaurar `U602`, `L602`, `R602` ou `R603` sem decisão explícita.
- O RTC usa CR1225 com suporte horizontal MYOUNG `BS-12-B4AK025-R` (LCSC `C20606807`).
- `J801` é o socket microSD SOFNG `TF-15x15` (LCSC `C111196`), com footprint e STEP locais.
- O BH1750 será uma placa filha externa em JST-XH de quatro vias e I²C3 (`PA8`/`PC9`). O dado do DHT22 está em `PA9`; firmware será alterado em trabalho separado.
- A PCB é vertical no abrigo Stevenson: entrada e conversores na parte inferior; DHT22, BMP280 e LoRa na parte superior. O corpo ventilado do DHT22 deve ultrapassar a borda da PCB; temperatura do BMP280 não é offset automático do DHT22.
- B.Cu é o plano contínuo preferencial de GND. Preserve retornos contínuos; não fragmente o plano por conveniência de roteamento.

## Layout, roteamento e validação

- A PCB é de duas camadas, FR-4 de 1,6 mm e cobre de 1 oz. Os limites globais atuais são 0,20 mm para trilha/clearance, 0,50 mm de cobre à borda e 0,20 mm de folga de furo.
- Aplique net classes e larguras conforme `DECISOES_IMPLEMENTACAO_PCB_2026-10.md`. `FINE_ESCAPE` não é padrão de roteamento; `RF_50R` só recebe largura após cálculo com o stackup da fabricante.
- Preserve laços curtos de buck, evite vias no nó SW e RF, e trate `VCAP_1`, cristais, referência analógica e ADC da biruta como regiões sensíveis.
- Pontos de teste devem sair como ramificações curtas e acessíveis, sem interromper a trilha principal ou invadir regiões RF, de cristal ou de comutação.
- Mudança de esquemático exige ERC; mudança de footprint, posicionamento ou roteamento exige DRC e revisão 3D; mudança de componente exige confrontar dados elétricos e mecânicos com o fabricante.
- Antes de compra/fabricação, regenere BOM e netlist, execute ERC/DRC finais, revise 3D/Gerbers e registre o que ainda depende de bancada.

## Versionamento

Versione fontes KiCad, bibliotecas locais necessárias, modelos 3D efetivamente referenciados, documentação de decisão e BOM regenerada. Não versione `tmp/`, caches, imagens de inspeção, netlists de teste, relatórios intermediários ERC/DRC, PDFs baixados temporariamente ou scripts pontuais de migração, salvo solicitação explícita.
