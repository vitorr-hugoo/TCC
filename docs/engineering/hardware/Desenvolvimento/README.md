# Índice de decisões de hardware

Esta pasta preserva as decisões de engenharia em documentos pequenos e rastreáveis. Para um relatório futuro unificado, trate o KiCad ativo como fonte de verdade e use estes arquivos como justificativa, cronologia e critérios de validação.

| Documento | Conteúdo | Estado no fluxo |
| --- | --- | --- |
| [Registro_de_alteracoes_hardware.md](Registro_de_alteracoes_hardware.md) | histórico consolidado de esquemático, bibliotecas e pendências | atualizado em 02/10/2026 |
| [DECISOES_IMPLEMENTACAO_PCB_2026-10.md](DECISOES_IMPLEMENTACAO_PCB_2026-10.md) | mudanças posteriores na alimentação, bateria, microSD, regras, aterramento e roteamento | registro de implementação/layout |
| [DECISAO_BH1750_PLACA_FILHA_I2C3_DHT22.md](DECISAO_BH1750_PLACA_FILHA_I2C3_DHT22.md) | BH1750 externo, I²C3 e mudança do dado do DHT22 | decisão elétrica e de integração |
| [DECISAO_DHT22_ABRIGO_TERMICO_BMP280.md](DECISAO_DHT22_ABRIGO_TERMICO_BMP280.md) | efeito térmico do abrigo Stevenson e posicionamento de DHT22/BMP280 | decisão térmica/mecânica |
| [ESTUDO_ALIMENTACAO_12V_PARA_3V3_SEM_ESTAGIO_5V.md](ESTUDO_ALIMENTACAO_12V_PARA_3V3_SEM_ESTAGIO_5V.md) | alternativas e justificativa para eliminar o estágio de 5 V | estudo; consultar o adendo para o resultado implementado |
| [PLANO_LAYOUT_PCB_2_CAMADAS.md](PLANO_LAYOUT_PCB_2_CAMADAS.md) | plano inicial de layout de duas camadas | plano histórico; o adendo registra as decisões que o substituem |

## Convenção de estado

- **Implementado**: consta nos arquivos KiCad ativos ou nas bibliotecas locais.
- **Diretriz aprovada**: deve orientar layout/roteamento, mas não garante que já foi desenhada na PCB.
- **Pendente de validação**: requer bancada, firmware, DRC/ERC final, revisão 3D ou confirmação de fornecedor.

Arquivos de SVG, netlist, ERC ou BOM exportados em outras pastas podem retratar uma revisão anterior. Eles não devem ser usados para compra ou fabricação sem regeneração a partir do projeto atual.
