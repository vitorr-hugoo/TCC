---
name: analise-estatica-c
description: Configura, executa e interpreta warnings, clang-tidy, cppcheck e análise MISRA de firmware C, com sanitizers em builds host quando aplicáveis. Use para diagnósticos e ferramentas, não para certificação.
---

# Análise estática de C

## Objetivo e limites

Obter diagnósticos reproduzíveis na configuração real do firmware e separar defeitos de
ruído. Use para configurar ferramentas ou investigar warnings. Não substitui testes/revisão
nem demonstra conformidade MISRA. Sanitizers são análise dinâmica complementar no host.

## Regras obrigatórias

Leia `AGENTS.md`, arquitetura, toolchain, flags e infraestrutura antes de configurar algo.
Preserve APIs, comportamento fora do escopo e configurações existentes; evite reformatação
em massa ou arquivos `.new`. Execute ferramentas existentes antes de introduzir novas.
Siga `padrao-c-embarcado` em correções C. Não invente build/testes/diagnósticos e declare
ferramentas ausentes, configurações não analisadas e falsos positivos ainda não resolvidos.
Não acrescente casts ou supressões para silenciar sintomas. Corrija causa somente dentro
do escopo autorizado; em pedido de diagnóstico, entregue achados sem editar firmware.

## Fluxo

1. Capture versão, MCU, linguagem, defines, includes, flags de ABI/FPU, fontes e variantes.
   Reutilize `compile_commands.json`; confira se reflete o build atual. Banco incompleto ou
   flags ARM não entendidas pelo Clang limitam a análise, não autorizam descartá-las em silêncio.
2. Execute build de desenvolvimento e registre warnings novos versus baseline da mesma
   configuração/toolchain. Código gerado/terceiros não deve receber correção de estilo em massa.
3. Avalie individualmente `-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow
   -Wundef -Wdouble-promotion -Wformat=2` em GCC/Clang compatíveis. Não aplique `-Werror`
   automaticamente ao legado; use gate acordado para CI/projeto novo.
4. Para clang-tidy, valide `.clang-tidy` com a versão instalada (`--verify-config` quando
   suportado e `--list-checks`); execute por fonte usando `-p` com o diretório do banco.
   A configuração inicial da raiz seleciona grupos para C e evita modernizações C++/desktop.
   Não use `--fix` na coleta inicial. Diagnósticos não implicam exit code não zero em toda
   ferramenta: configure política de falha explicitamente; o wrapper usa `--warnings-as-errors=*`.
5. Para cppcheck, use `--project=compile_commands.json`, plataforma correspondente ao target
   quando aplicável e includes/modelos de biblioteca corretos. Analise variantes condicionais
   importantes. Habilite unusedFunction em análise de programa completo quando suportada;
   interrupções/callbacks podem parecer não referenciados. Não exclua fontes da aplicação
   junto com vendor. O wrapper define `--error-exitcode=1`.
6. Categorize cada diagnóstico: defeito com evidência, falso positivo fundamentado, lacuna
   de configuração ou recomendação. Registre ferramenta, regra, arquivo/linha, impacto e ação.
7. Para lógica host, considere ASan/UBSan com flags compatíveis de compilação **e link**,
   por exemplo `-fsanitize=address,undefined -fno-omit-frame-pointer`. Execute testes reais;
   não presuma disponibilidade no MCU ou que um build instrumentado foi executado.
8. Repita somente verificações afetadas pela correção. Preserve baseline e registre diferenças.

## MISRA

Use verificador configurado pelo projeto, edição aplicável e registro formal de desvios.
Cppcheck pode oferecer addon/integração MISRA conforme versão, configuração e licenças;
não trate execução padrão como cobertura MISRA. Não copie texto protegido da especificação.
“Nenhum diagnóstico encontrado” não significa “MISRA compliant”: conformidade requer
processo formal, cobertura das regras/diretivas, ferramenta apropriada e desvios rastreáveis.

## Exemplos e checklist

Exemplo: warning de narrowing exige provar intervalo antes de converter e testar limites.
Anti-exemplo: inserir cast para o diagnóstico sumir sem verificar o valor intermediário.

- [ ] Build/compilation database correspondem ao target e variante informados.
- [ ] Conversões, fluxo de controle, memória e código não utilizado foram avaliados.
- [ ] Configuração validada na versão real, comandos e saídas preservados.
- [ ] Baseline não foi ampliado silenciosamente para esconder novos achados.
- [ ] Supressões existentes têm motivo; novas exigem justificativa individual conforme projeto.
- [ ] Falhas de ferramenta não viraram “zero problemas”.

## Saída e relações

Entregue tabela ferramenta/versão, comando, resultado, diagnósticos novos, limitações e ações.
`code-review-embarcado` interpreta impacto no sistema; `automacao-testes` mantém wrappers;
`validacao-firmware` agrega o resultado. Templates da raiz são copiados/adaptados somente se
o projeto não tiver equivalente. Esta skill continua utilizável sem os wrappers do pacote.

Referências oficiais para conferir opções da versão usada:
[clang-tidy](https://clang.llvm.org/extra/clang-tidy/),
[cppcheck](https://cppcheck.sourceforge.io/manual.pdf),
[warnings GCC](https://gcc.gnu.org/onlinedocs/gcc/Warning-Options.html).
