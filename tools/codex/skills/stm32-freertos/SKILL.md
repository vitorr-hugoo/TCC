---
name: stm32-freertos
description: Orienta implementação e diagnóstico de firmware STM32 com HAL, LL, CMSIS, FreeRTOS ou CMSIS-RTOS2, incluindo CubeMX e bare-metal. Use para integração de MCU, periféricos e concorrência.
---

# STM32, bare-metal e RTOS

## Objetivo e limites

Integrar lógica ao MCU respeitando inicialização, ownership e requisitos temporais. Use quando
o código depende de STM32/CMSIS/HAL/LL/RTOS; não use para lógica C pura nem generalize
características de um STM32 a todas as famílias. Não substitui manual, errata ou medição real.

## Regras obrigatórias e descoberta

Leia `AGENTS.md`, `.ioc`, startup/linker, configuração de clocks, interrupções, HAL e RTOS.
Identifique MCU/revisão, placa, versão do pacote Cube/CubeIDE, port do kernel, cache/MPU e
memória acessível por DMA. Preserve APIs e comportamento fora do escopo, sem reformatação
em massa. Execute/reutilize ferramentas existentes antes de introduzir novas. Todo C segue
`padrao-c-embarcado`. Relate limites e não invente build, testes ou conformidade MISRA.

## Fluxo de implementação

1. Mapeie quem inicializa, usa e encerra cada recurso; registre contextos task/ISR/DMA.
2. Proponha mudança mínima e estados seguros em erro/reset. Confira datasheet/reference
   manual/errata da família e fontes locais da versão utilizada.
3. Preserve regiões `USER CODE BEGIN/END`; mantenha módulos próprios fora do gerado quando
   útil. Não altere `.ioc` sem necessidade. Se a configuração exigir regeneração, informe
   risco de sobrescrita e compare o diff gerado; não edite HAL/CMSIS/vendor por estilo.
4. Trate `HAL_OK`, `HAL_ERROR`, `HAL_BUSY`, `HAL_TIMEOUT` quando retornados. Estabeleça
   retry limitado/recuperação conforme contrato; não transforme qualquer erro em loop infinito.
   Em LL, verifique flags/sequências/timeout explicitamente; não presuma proteção da HAL.
5. Aplique o modo de execução e a checklist de periféricos abaixo; compile/teste o disponível.
6. Relate configuração, arquivos, evidência temporal/memória e validações pendentes na placa.

## Bare-metal

No superloop, divida trabalho em passos limitados e máquinas de estado; ISRs sinalizam
eventos. Dimensione latência de polling e bursts. Proteja acesso compartilhado com o mecanismo
adequado ao core, preservando o estado prévio de interrupções ao sair de seção crítica.
Não introduza RTOS para resolver uma espera simples. Confirme startup, vetor, clocks, linker,
stack e condições de baixo consumo; o tick pode parar, invalidando timeout baseado nele.

## Tasks, recursos e periodicidade

- Defina prioridades por deadline/latência e dependências, não por preferência. Verifique
  starvation, inversão de prioridade e ordem global de aquisição de mutexes.
- Mutex protege ownership em contexto de task; semáforo sinaliza/conta eventos. Fila copia
  itens conforme tamanho configurado; fila de ponteiros exige lifetime/ownership explícitos.
  Event flags podem combinar/coalescer eventos e não substituem contador de ocorrências.
- Software timers executam callbacks em contexto definido pelo backend; evite trabalho
  bloqueante que impeça outros timers. Documente timeouts, filas cheias e recuperação.
- `osDelay()`/`vTaskDelay()` são relativos; `osDelayUntil()` e API periódica FreeRTOS da
  versão mantêm referência temporal. Defina política de overrun: recuperar fase, pular ciclo
  ou entrar em falha. Não acumule ciclos atrasados ilimitadamente.
- CMSIS-RTOS2 usa **ticks**, não milissegundos, nos delays. Converta com frequência real,
  arredondamento e intermediário suficiente. No FreeRTOS avalie `pdMS_TO_TICKS`, resolução
  e overflow da versão. Período positivo em ms pode virar zero ticks se mal convertido.
- `osDelayUntil` recebe tick absoluto e trata rollover sob limites da API (intervalo máximo
  de 2^31-1 ticks); cheque retorno e prazo já perdido. Não use `agora + periodo` recalculado
  após cada execução como substituto de uma referência acumulada.

### Stack

Considere arrays/estruturas locais, profundidade, ISR conforme port, `printf`, bibliotecas e
ponto flutuante. Não escolha tamanho arbitrário. `osThreadAttr_t.stack_size` é em bytes;
profundidade de stack FreeRTOS costuma ser em elementos `StackType_t`, conferir port/API.
Converta explicitamente e registre margem. Watermark mede uso observado, não pior caso
comprovado; exercite caminhos de erro e combine com análise de chamadas/mapa.

### ISR e DMA

Mantenha ISR curta, sem bloqueio ou logging pesado; delegue trabalho. FreeRTOS usa variantes
`FromISR` e pedido de troca de contexto conforme port. CMSIS-RTOS2 usa suas próprias APIs,
somente as documentadas como permitidas em ISR, com timeout zero quando exigido; não invente
nomes CMSIS `FromISR`. Verifique a implementação do adaptador instalada.

Em Cortex-M, menor valor numérico de prioridade significa maior urgência. Confira bits
implementados, grouping e limites de syscall do port; não confunda prioridade de task com
NVIC nem constantes deslocadas de registrador com valores passados à HAL/CMSIS. Interrupção
acima do limite permitido não pode chamar o kernel nem pela variante ISR.

`volatile` não garante atomicidade nem sincronização. Para DMA, defina transferência de
ownership, conclusão/erro e lifetime. Confirme memória acessível, alinhamento e coerência.
Em MCU com D-cache, aplique manutenção segundo direção, linhas completas, MPU e documentação
da família; evite invalidar dados CPU vizinhos sujos. Não prescreva clean/invalidate universal
nem suponha que barreiras, sozinhas, resolvem coerência de cache.

## Checklist de periféricos

- GPIO: clock, polaridade, pull/AF e saída segura antes de habilitar potência.
- ADC/DAC: referência, resolução, trigger, calibração quando suportada, escala e over/underrun.
- UART: configuração, recepção parcial, framing, overrun e prazo de recuperação.
- SPI: modo, chip select, largura e conclusão real; I2C: formato de endereço da API,
  NACK/busy, clock stretching e recuperação limitada.
- Timers/PWM: clock efetivo conforme árvore do MCU (não presumir clock APB direto), PSC/ARR,
  duty, preload, overflow, dead time e break quando disponíveis.
- DMA: ownership, comprimento, cache, modo circular e detecção de perda de amostras.
- Watchdog: saúde/progresso de todos os componentes críticos antes de alimentar. Nunca
  adicionar alimentação a um loop travado para ocultar a falha.
- RTC quando usado: domínio backup, fonte, validade do calendário e reset.
- Tick/timeout: rollover por diferença unsigned com intervalo/observação válidos; HAL tick
  e RTOS tick podem ser fontes distintas. `HAL_Delay` em ISR pode bloquear o tick necessário.

## Exemplo e saída

Para aquisição periódica, defina período em ticks, referência inicial, atualização da próxima
liberação a cada ciclo e política de atraso. Desperte processamento por fila com capacidade
dimensionada para bursts; teste fila cheia. Anti-exemplo: processamento longo em callback DMA
seguido de `osDelay(1000)` presumindo um segundo e API permitida em ISR.

Entregue decisão, arquivos/configurações, contextos e ownership, unidades, erros tratados,
comandos executados e medições pendentes. Para testes consulte `testes-unitarios-embarcados`;
para revisão completa, `code-review-embarcado`; a orquestração pertence a `validacao-firmware`.

Confira a API instalada com as referências oficiais:
[delays CMSIS-RTOS2](https://arm-software.github.io/CMSIS_6/v6.0.0/RTOS2/group__CMSIS__RTOS__Wait.html),
[header CMSIS-RTOS2](https://github.com/ARM-software/CMSIS_6/blob/main/CMSIS/RTOS2/Include/cmsis_os2.h),
[prioridades FreeRTOS Cortex-M](https://www.freertos.org/Documentation/02-Kernel/03-Supported-devices/04-Demos/ARM-Cortex/RTOS-Cortex-M3-M4).
