---
name: code-review-embarcado
description: Revisa alterações de firmware C, bare-metal ou RTOS, identifica riscos técnicos e produz relatório por severidade. Use para revisão; não corrige código sem pedido explícito.
---

# Revisão técnica de firmware

## Objetivo, escopo e limites

Responder se a alteração introduz bugs, riscos ou problemas arquiteturais demonstráveis.
Use em revisão de diff, módulo ou firmware. Não use como substituto de build, teste,
análise estática ou certificação. Não execute correções durante uma revisão solicitada
como somente leitura, inclusive ajustes cosméticos sugeridos por outra skill.

## Regras obrigatórias

- Entenda primeiro `AGENTS.md`, arquitetura, requisitos, MCU, toolchain e contexto de execução.
- Preserve APIs e comportamento fora do escopo; não reformate em massa.
- Reutilize ferramentas e evidências existentes antes de propor infraestrutura nova.
- Consulte `padrao-c-embarcado` para estilo e todo C sugerido; não replique sua política.
- Não invente execução, contagens, build ou conformidade MISRA. Informe limites da inspeção.
- Separe defeito comprovado de hipótese e de preferência arquitetural. Mostre o caminho
  de execução e as condições necessárias; incerteza não deve virar afirmação de bug.

## Fluxo

1. Delimite revisão: revisão/commit ou arquivos, requisitos afetados e áreas não inspecionadas.
2. Leia o diff e seus chamadores, contratos, configuração, linker e callbacks relevantes.
3. Percorra a checklist abaixo. Use resultados de `analise-estatica-c` como evidência;
   solicite/aplique `testes-unitarios-embarcados` se uma reprodução for parte do pedido.
4. Classifique achados por consequência e plausibilidade, sem duplicar a mesma causa.
5. Entregue relatório com localização verificável e limitações. Somente corrija se solicitado.

## Checklist técnica

- **C:** inicialização em todos os caminhos, comportamento indefinido, overflow/underflow,
  casts, conversões implícitas, signed/unsigned, truncamento, tipos e promoções intermediárias;
  efeitos colaterais, precedência, divisão por zero, shifts negativos ou além da largura,
  dead code e trechos inalcançáveis. Overflow signed não equivale ao wrap unsigned.
- **Memória:** limites, `NULL`, ponteiros inválidos, lifetime, alinhamento, aliasing,
  arrays locais grandes, stack, recursão, alocação/fragmentação e falhas de alocação.
  Confira unidade/tamanho e sobreposição em `memcpy`, limites em `memset`, terminação
  em `strcpy`, capacidade em `sprintf`/equivalentes e retorno/truncamento em versões limitadas.
- **Concorrência:** ownership e acessos por task, ISR, DMA e hardware; operações compostas,
  atomicidade, seções críticas, mutexes, semáforos, filas, ordem de locks, deadlocks e inversão
  de prioridade. `volatile` não garante atomicidade e não substitui sincronização.
- **RTOS:** `osDelay`, `osDelayUntil`, `vTaskDelay`, `vTaskDelayUntil` e equivalente da versão;
  unidades, drift, jitter, deadline, overrun, prioridades, starvation, stack, timeouts e bloqueios
  indefinidos. Inspecione mutex/semaphore/queue/event flags/timers, perda de eventos e
  operações bloqueantes. Delay relativo após trabalho acumula seu tempo no período.
- **ISR:** duração, latência, prioridade, acesso a periféricos, logging/`printf`, chamadas
  bloqueantes e APIs permitidas. Faça o mínimo necessário e delegue processamento pesado.
  Consulte `stm32-freertos` para restrições do port e da camada CMSIS.
- **DMA:** ownership, lifetime, alinhamento, memória acessível pelo controlador, conclusão,
  cache quando existente e concorrência CPU/DMA. Não reutilizar buffer durante transferência.
- **Watchdog:** loops infinitos/longos e bloqueios que impedem progresso. Alimentar o watchdog
  indiscriminadamente dentro de loop travado mascara falha; exija evidência de saúde/progresso.
- **Temporização:** unidade, fonte e frequência do tick, suspensão em baixo consumo,
  rollover, limites de intervalo, timers e comparações de contadores.
- **Estados:** estados inválidos, transições impossíveis/não tratadas, estado de erro,
  recuperação, `default` tecnicamente necessário e mudança concorrente de estado.
- **Arquitetura:** acoplamento, dependências circulares, responsabilidades misturadas,
  lógica de aplicação em drivers ou dependência HAL desnecessária, APIs frágeis e testabilidade.
  Preferência pessoal não é bug funcional.

### Periféricos

Para cada periférico afetado, confira configuração, estado inicial/reset, sequência de
inicialização, retornos HAL, timeout, erro, limites, unidades e dependências da placa.

#### GPIO
Polaridade, pull, função alternativa, nível seguro antes de habilitar saída e escrita concorrente.
#### ADC
Referência, resolução, tempo de amostragem, calibração, sequência, overrun e escala física.
#### DAC
Faixa, referência, trigger, valor inicial seguro e underrun.
#### UART
Baud, framing, erros/overrun, recepção parcial, comprimento, timeout e recuperação.
#### SPI
CPOL/CPHA, largura, chip select, full duplex, dummy bytes e término efetivo no barramento.
#### I2C
Endereço conforme API, ACK/NACK, busy, clock stretching e recuperação com prazo limitado.
#### Timers
Clock real, prescaler, ARR, largura, captura, overflow e cálculo de frequência.
#### PWM
Frequência/duty, limites, preload, dead time, break e saída segura em falha/reset.
#### DMA
Modo circular/normal, índices produtor/consumidor e checklist de ownership/cache acima.
#### Watchdog
Janela, tolerância do clock, reset reason e critérios reais de progresso.
#### RTC
Quando presente: fonte, domínio backup, validade inicial, calendário e leitura consistente.

## Exemplos e anti-exemplos

`HAL_GetTick() < tempoInicial + timeout` pode falhar no rollover. Avalie diferença unsigned
no tipo do contador: `(uint32_t)(HAL_GetTick() - tempoInicial) < timeout`.
Isso exige intervalo representável, tick que avance e observação antes de uma volta completa;
não resolve suspensão do tick nem esperas sem progresso. Investigue as premissas primeiro.

“Trocar HAL por LL porque é melhor” é preferência sem evidência. “A espera bloqueante ocorre
na ISR que impede o tick de avançar, portanto seu timeout não expira” é um achado quando o
grafo de chamadas e prioridades confirmam a condição.

## Severidade e formato de saída

- **CRÍTICO:** corrupção de memória, UB grave, travamento, dano físico potencial, falha de
  segurança ou perda de controle com caminho causal sustentado pelo contexto.
- **ALTO:** erro funcional/falha em condições plausíveis.
- **MÉDIO:** robustez, arquitetura/manutenção ou falha em condição menos comum.
- **BAIXO:** melhoria sem risco funcional imediato.
- **ESTILO:** violação de `padrao-c-embarcado`.

Comece exatamente com as contagens dos achados reportados:

```text
Resumo
Críticos: X
Altos: X
Médios: X
Baixos: X
Estilo: X

[SEVERIDADE] PROBLEMA ou RECOMENDAÇÃO
Arquivo: caminho e linha
Função: nome ou não aplicável
Trecho: trecho mínimo
Problema: condição e evidência (explicitar hipótese quando houver)
Consequência: impacto observável
Correção recomendada: intervenção localizada e validação sugerida
```

Finalize com escopo, evidências consultadas, verificações não executadas e dúvidas restantes.
Ausência de achados não prova ausência de defeitos. `validacao-firmware` consome este relatório;
esta skill não chama a orquestradora, evitando ciclo de execução.
