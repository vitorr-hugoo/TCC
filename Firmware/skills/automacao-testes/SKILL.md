---
name: automacao-testes
description: Cria automações Python para build, testes, análise, logs e integração/HIL de firmware, com timeouts e códigos de saída para CI. Use para scripts de engenharia, não para definir regras de código C.
---

# Automação de testes

## Objetivo, escopo e limites

Transformar verificações existentes em comandos reproduzíveis localmente e em CI. Abrange
build, runners, analisadores, formatação, UART/USB, logs, relatórios, integração e HIL.
Não use para inventar critérios técnicos de aprovação nem implantar infraestrutura paralela
quando já houver equivalente. Hardware exige identificação da placa e procedimento do projeto.

## Regras obrigatórias

Primeiro leia `AGENTS.md`, arquitetura, comandos e infraestrutura existentes. Reutilize-os antes
de adicionar dependências. Preserve APIs, comportamento fora do escopo e arquivos não envolvidos;
evite reformatação em massa. C eventualmente criado segue `padrao-c-embarcado`.
Não invente testes/build executados ou conformidade MISRA; explicite dependências e limites.

- Python 3, `main() -> int`, `argparse`, `pathlib`, parâmetros de caminhos e mensagens úteis.
- `0` significa sucesso; qualquer outro valor é falha/pendência para CI, documentando a distinção.
- Use `subprocess` com lista de argumentos e `shell=False`, verifique `returncode` e preserve
  saída útil. Não transforme timeout, executável ausente ou erro de parsing em PASS.
- Defina prazo finito, diretório explícito e cleanup de recursos. Para ferramentas que geram
  filhos, estabeleça encerramento da árvore conforme o SO; os wrappers básicos do pacote
  limitam o processo direto e não são supervisores de sessões HIL.
- Não parseie logs por um “PASS” genérico; prefira exit code mais relatório estruturado do
  runner. Detecte zero testes. Não derive contagem de testes do número de comandos executados.
- Não grave credenciais em logs; caminhos no template devem ser relativos ao projeto.

## Fluxo

1. Descubra comando real e critério de sucesso/falha da ferramenta; rode-o antes de envolvê-lo.
2. Parametrize raiz, artefatos, timeout e comando. Declare dependências externas e versões.
3. Implemente propagação de erro, tratamento de exceções e relatório legível/estruturado.
4. Verifique sucesso, falha, ferramenta ausente, timeout e caminhos com espaços em isolamento.
5. Integre ao pipeline existente; falha obrigatória impede aprovação. Não escreva CI de um
   fornecedor específico se só for necessário um comando portátil.

## HIL

Fluxo: compilar → gravar MCU identificado → reiniciar → abrir UART/USB → estabelecer estado
conhecido → estimular → capturar → validar → PASS/FAIL → cleanup. A abertura da UART antes
do reset pode ser necessária para capturar boot; adapte e registre a ordem.

Parametrize serial/identificador da placa, baud, firmware e versão do protocolo. Exija acesso
exclusivo ao target, prazo por etapa e prazo global; descarte dados antigos só quando isso
não eliminar a resposta esperada. Use relógio monotônico, framing explícito e limite de bytes.
No `finally`, feche portas e coloque estímulos/atuadores em estado seguro conforme projeto.
Não grave uma placa escolhida automaticamente por ser a primeira encontrada. Relate setup
indisponível como não executado; não tente hardware durante teste unitário host.

## Exemplos e anti-exemplos

No pacote completo, `scripts/executar_testes.py --raiz . -- ctest --test-dir build --output-on-failure --no-tests=error`
envolve o runner existente. Para outra ferramenta, use seu mecanismo de falha em zero testes.
O wrapper preserva a saída; sucesso do comando sozinho não prova quantidade de casos.

Anti-exemplos: `shell=True` para concatenar parâmetros; `except Exception: return 0`;
esperar UART sem prazo; declarar sucesso porque o processo iniciou; alimentar watchdog no
teste para ocultar travamento funcional.

## Checklist e saída

- [ ] CLI não interativa, paths portáteis, dependências documentadas e exit code verificável.
- [ ] Falhas, timeout, cleanup e saída útil testados; nenhum resultado fictício.
- [ ] Relatório distingue PASS, FAIL, NÃO CONFIGURADO, NÃO DISPONÍVEL e NÃO EXECUTADO.
- [ ] HIL tem target identificado, isolamento e estado seguro; cobertura host não o substitui.

Entregue arquivos, uso da CLI, requisitos, verificações executadas e limitações. Os scripts
da raiz são templates reutilizáveis, não dependências necessárias para aplicar esta skill
isoladamente. `testes-unitarios-embarcados` define casos; `analise-estatica-c` configura
diagnósticos; `validacao-firmware` decide completude e `code-review-embarcado` avalia riscos.
