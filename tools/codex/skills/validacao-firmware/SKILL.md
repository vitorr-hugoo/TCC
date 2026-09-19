---
name: validacao-firmware
description: Orquestra formatação, build, warnings, análise, testes e revisão para avaliar se uma alteração de firmware está validada, registrando falhas e verificações ausentes. Use para avaliação final de prontidão.
---

# Validação de firmware

## Objetivo, escopo e limites

Responder se uma alteração está tecnicamente validada para o escopo e critérios do projeto.
Use ao integrar/finalizar alterações. Não use como substituto de uma competência específica,
aprovação regulatória, autorização para publicar ou prova de comportamento não medido.

## Regras obrigatórias

Entenda `AGENTS.md`, arquitetura e requisitos primeiro. Preserve APIs, comportamento fora do
escopo e infraestrutura existente; sem reformatação em massa. Execute ferramentas disponíveis
antes de propor novas. Código C segue `padrao-c-embarcado`. Não declare teste/build executado
sem evidência, nem conformidade MISRA. Evidência deve corresponder à revisão, configuração e
arquivos finais; modificação posterior invalida verificações afetadas.

## Pipeline e responsabilidades

1. **Formatação:** política `padrao-c-embarcado`, configuração do projeto e verificador sem escrita.
2. **Build:** comando/toolchain/variante do projeto; capture log e exit code.
3. **Warnings:** `analise-estatica-c`, comparando baseline compatível; build com exit zero não
   significa zero warnings.
4. **Análise estática:** `analise-estatica-c`, ferramentas configuradas e cobertura de variantes.
5. **Testes unitários:** `testes-unitarios-embarcados`, casos pertinentes e contagem real do runner.
6. **Testes automatizados:** `automacao-testes`, scripts existentes e validação de logs/resultados.
7. **Integração/HIL:** `automacao-testes` com plano/placa disponíveis; `stm32-freertos` para
   aspectos dependentes de MCU, RTOS e temporização. Teste host não substitui etapa de placa exigida.
8. **Code review:** `code-review-embarcado`, relatório por severidade, normalmente somente leitura.
9. **Relatório final:** agregue evidências e aplique critérios abaixo.

Antes de executar, determine etapas obrigatórias por requisito/risco e contrato do projeto.
Não rebaixe etapa a opcional após falhar. Se ferramenta está ausente, marque NÃO DISPONÍVEL;
se falta configuração, NÃO CONFIGURADO; se não executada, NÃO EXECUTADO. NÃO APLICÁVEL
exige justificativa técnica. Nenhuma dessas marcações equivale a PASS.

Etapas podem continuar para diagnóstico se independentes; após build falhar, não execute teste
dependente de binário antigo. Associe logs ao commit/diff, versão e variante, incluindo mudanças
não commitadas. Se uma skill especializada não estiver instalada, registre limitação e aplique
infraestrutura documentada disponível; não invente que a carregou. Não há chamadas de volta
das competências para esta orquestradora.

## Decisão final

Avalie nesta ordem:

1. **REPROVADO:** qualquer CRÍTICO; ALTO sem justificativa/aceite rastreável permitido pelo
   projeto; build falho; teste ou outro gate obrigatório falho. Risco crítico não é dispensado.
2. **NÃO FOI POSSÍVEL VALIDAR:** sem falha decisiva conhecida, mas falta evidência obrigatória
   (incluindo build ou revisão exigidos), ambiente/configuração inválidos ou execução incompleta.
3. **APROVADO COM RESSALVAS:** todas as etapas obrigatórias concluídas com sucesso e riscos
   bloqueantes resolvidos, mas há verificações opcionais ausentes ou recomendações/riscos
   residuais aceitos e documentados. ALTO justificado nunca vira aprovação sem ressalvas.
4. **APROVADO:** todas as etapas aplicáveis aprovadas, nenhuma pendência ou ressalva aberta.

## Exemplo de saída

```text
VALIDAÇÃO DO FIRMWARE
Revisão/configuração: identificação exata
Formatação: PASS — evidência
Build: PASS — comando e log
Warnings: PASS — baseline e novos diagnósticos
Análise estática: NÃO DISPONÍVEL — ferramenta ausente
Testes unitários: PASS — contagem somente se obtida do runner
Testes automatizados: PASS — relatório
HIL: NÃO EXECUTADO — motivo e obrigatoriedade
Code review: 0 crítico / 0 alto / 2 médios — relatório
Resultado: NÃO FOI POSSÍVEL VALIDAR
Motivo: análise estática obrigatória não executada
Pendências: responsável/ação e evidência necessária
```

Anti-exemplo: “APROVADO” porque os comandos disponíveis passaram, ignorando build ausente.

## Checklist e automação

- [ ] Critérios/obrigatoriedade definidos antes da execução, revisão final identificada.
- [ ] Logs, versões, comandos e resultados rastreáveis; ausências explícitas.
- [ ] Riscos críticos/altos tratados e dependências de hardware verificadas quando exigidas.
- [ ] Nenhuma contagem ou aprovação inferida de ausência de saída.
- [ ] Limitações e próxima ação informadas sem confundir pendência com sucesso.

No pacote completo, `scripts/validar_firmware.py` executa gates declarados em JSON e agrega
status mecânico. O script **não faz revisão semântica**, não classifica riscos e não concede
aprovação técnica; essa decisão requer esta skill e relatório de revisão. Consulte o
`README.md` da raiz para contrato e exemplo de configuração. As skills permanecem úteis
isoladamente usando comandos equivalentes do projeto.
