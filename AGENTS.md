# TCC — Estação Meteorológica

## Mapa do repositório

- `hardware/` contém a PCB própria, projeto KiCad, bibliotecas locais, BOM e documentação de engenharia.
- `firmware/` contém o firmware STM32, gateway ESP32, testes e protocolos.
- `docs/` contém a documentação compartilhada do TCC e os registros históricos de decisão.
- `tools/` contém automações e skills do projeto.
- `tmp/` contém artefatos temporários e não deve ser versionado.

## Antes de qualquer alteração

1. Execute `git status --short` e preserve alterações preexistentes.
2. Use os arquivos-fonte ativos para confirmar o estado atual; relatórios, SVGs, netlists, imagens e BOMs exportadas podem ser históricos.
3. Não use operações destrutivas de Git e não descarte trabalho do usuário sem solicitação explícita.

## Encaminhamento por domínio

- Para qualquer tarefa em `hardware/**`, leia [hardware/AGENTS.md](hardware/AGENTS.md) antes de analisar em profundidade ou editar arquivos.
- Para qualquer tarefa em `firmware/**`, leia [firmware/AGENTS.md](firmware/AGENTS.md) antes de analisar em profundidade ou editar arquivos.
- Para uma tarefa que atravessa os dois domínios, descreva primeiro a interface afetada — pinagem, tensão, barramento, protocolo ou formato de dados — e separe as mudanças e commits por domínio sempre que possível.

## Regras compartilhadas

- Não altere hardware ao executar uma tarefa de firmware, nem firmware ao executar uma tarefa de hardware, salvo pedido explícito.
- Mudanças de pinagem, I²C/SPI/UART, níveis de tensão, sensores, interrupções ou protocolo devem ser documentadas nos dois lados afetados; a implementação em cada domínio continua dependente do escopo solicitado.
- Atualize a documentação de engenharia quando uma decisão técnica for modificada.
- Não inclua em commits caches, arquivos de `tmp/`, relatórios intermediários, exports experimentais, arquivos de validação temporários ou credenciais.
- Antes de commitar, confira `git diff --cached --check` e os arquivos staged. Para compra ou fabricação, regenere BOM e netlist a partir das fontes ativas.
