---
name: testes-unitarios-embarcados
description: Cria e executa testes unitários determinísticos de lógica de firmware C isolada do hardware, preservando o framework existente. Use para algoritmos, estados, protocolos e erros; não substitui HIL.
---

# Testes unitários embarcados

## Objetivo e escopo

Demonstrar contratos observáveis da lógica sem placa real. Use para algoritmos, cálculos,
filtros, conversões, máquinas de estado, protocolos, parâmetros, erros, limites e exceções,
nessa prioridade conforme o risco. Teste drivers quando houver abstração suficiente.
Não use teste host para afirmar correção elétrica, timing real, cache/DMA ou prioridade de ISR.

## Regras obrigatórias

Entenda `AGENTS.md`, arquitetura e contratos antes de editar. Preserve APIs e comportamento
fora do pedido; evite reformatação em massa. Execute/reutilize infraestrutura existente antes
de adicionar ferramentas. Todo C, inclusive testes e doubles, respeita `padrao-c-embarcado`,
salvo nomes/assinaturas exigidos pelo framework. Registre limitações; nunca invente resultados,
build executado ou conformidade MISRA.

Testes devem ser determinísticos, independentes, rápidos, reproduzíveis e sem hardware ou
ordem obrigatória. Injete relógio/entradas; não use sleeps reais, aleatoriedade sem seed ou
estado residual. Reinicialize estado e doubles por teste. O oráculo vem do requisito,
não de copiar o algoritmo em teste.

## Fluxo

1. Identifique framework, comando, contratos e regressão a prevenir. Preserve o existente;
   Unity, Ceedling e CMock são opções para projetos sem padrão, não dependências obrigatórias.
2. Faça matriz requisito → entrada/cenário → resultado/efeito esperado → teste.
3. Separe lógica de hardware na menor fronteira útil: aplicação → interface ADC → HAL ADC.
   Use injeção por interface, link ou ponteiro de função conforme arquitetura; não redesenhe
   todo driver só para mockar cada chamada.
4. Escolha stub para retorno controlado, mock para interação/ordem relevante e fake para
   comportamento simplificado (fila, relógio, armazenamento). Não simule registradores como
   prova de comportamento da placa. Evite expectativas sobre detalhes internos sem contrato.
5. Implemente casos, execute comando existente e registre resultado real. Quando possível,
   confirme que a regressão falha antes e passa depois da correção.
6. Avalie cobertura como indicador de lacunas, sem confundir percentual com qualidade.
   Considere sanitizers host com `analise-estatica-c` e limite diferenças de ABI/endianness.

## Checklist de casos

- Mínimo, máximo, zero, abaixo/exatamente/acima do limite; intermediários que transbordam.
- Timeout com relógio fake, antes/no/depois do prazo e rollover com suas premissas.
- Erro de periférico, retorno inesperado, retry limitado e estado seguro após falha.
- `NULL` quando permitido como entrada inválida, buffer vazio/cheio, comprimento excedente,
  pacote parcial, checksum inválido e ausência de escrita além da capacidade.
- Transições válidas, inválidas, estado desconhecido, recuperação e efeitos colaterais.
- Filtros: inicialização, degrau, saturação e tolerância numérica derivada do requisito.
- Reset dos doubles, independência, nomes descritivos e expectativa verificável.

## Exemplo de cenário reutilizável

Para um módulo que desliga aquecimento somente **acima** do limite: injete uma leitura
exatamente no limite e verifique saída mantida; injete limite + resolução e verifique saída
desligada. Um stub entrega a temperatura; um mock verifica o comando de desligamento se
esse for o contrato público. Um fake de relógio permite avançar até timeout sem esperar.

Anti-exemplo: testar só o valor nominal e exigir cinco chamadas internas de HAL sem que
essa contagem faça parte do contrato. Outro: o teste retorna sucesso mesmo sem ser descoberto.
Configure o runner para falhar se nenhum teste for coletado ou confira sua contagem real.

## Saída e relações

Informe arquivos alterados, matriz de casos, comando/diretório/versões, quantidade real de
testes aprovados/falhos quando o runner a disponibiliza, logs, cobertura disponível e lacunas
que exigem integração/HIL. `automacao-testes` empacota execução; `code-review-embarcado`
avalia riscos não cobertos; `validacao-firmware` agrega evidências sem ser chamada de volta.
