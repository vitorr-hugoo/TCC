# Validação final do firmware

Revisão: worktree local em 12/09/2026, incluindo a migração estrutural dos arquivos próprios, o isolamento da falha de ADC, a configuração do build STM32, a remoção de credenciais do sketch versionável e o endurecimento do runner ESP32 contra artefatos antigos.

## Resultado: REPROVADO

O build obrigatório do gateway não concluiu a partir de saída limpa. A aprovação técnica também permanece bloqueada pela credencial previamente versionada, que precisa ser rotacionada, e pela análise estática STM32 sem banco de compilação. O agregador final registrou esses estados em `relatorios/validacao.json`.

## Etapas

| Etapa | Obrigatória | Resultado | Evidência |
| --- | --- | --- | --- |
| Formatação | Sim | PASS | 45 arquivos próprios verificados com clang-format 14.0.0. |
| Build STM32 | Sim | PASS | Debug e Release com GNU Make 4.4.1 e ARM GCC 14.3.1. |
| Build gateway ESP32 | Sim | FAIL | Arduino CLI 1.2.0 não confirmou conclusão em 300 s após limpeza da saída, com 1 e 4 jobs. |
| Warnings | Sim | PASS parcial | 0 warnings encontrados e 0 novos nos logs STM32 atuais; o gateway não produziu log de compilação concluída. |
| Análise estática | Sim | NÃO CONFIGURADO | clang-tidy 18.1.8 valida a configuração, porém não há `compile_commands.json` STM32; cppcheck não está disponível. |
| Testes unitários | Sim | PASS | 78/78 sensores/integração e 54/54 LoRa/gateway. |
| Testes automatizados | Não | PASS | Cenário `radio_ff`: 1/1. |
| Integração/HIL | Não | NÃO CONFIGURADO | Não há placa, porta serial, procedimento de gravação ou bancada definida para esta execução. |
| Revisão técnica | Sim | PASS com ressalvas | 0 críticos, 1 alto e 2 médios; ver `relatorios/revisao-tecnica.md`. |

## Evidências produzidas

- `relatorios/build/Debug.log` e `relatorios/build/Release.log` registram os builds STM32 atuais.
- `relatorios/build/gateway.log` registra o timeout do build limpo ESP32; a saída parcial foi removida pelo runner.
- `relatorios/testes-unitarios.json` e `relatorios/testes-automatizados.json` correspondem ao worktree final.
- `python tests/verificar_formatacao.py --raiz . --proprios` passou para as 45 fontes explicitamente selecionadas.
- A suíte de sensores extrai `MX_ADC1_Init` do fonte atual e cobre falha de `HAL_ADC_Init`, falha de `HAL_ADC_ConfigChannel` e inicialização bem-sucedida.

## Problemas e pendências

- ALTO: rotacionar a credencial Wi-Fi que já esteve no histórico e atualizar somente `Gateway ESP32/gateway_credentials.h`, ignorado pelo Git. Avaliar a remoção controlada do segredo do histórico se o repositório foi compartilhado.
- BLOQUEANTE DE VALIDAÇÃO: corrigir a instalação Arduino/ESP32 até que um build limpo do gateway conclua sem timeout. Não aceitar o binário de execução anterior.
- ALTO PARA EVIDÊNCIA: gerar `compile_commands.json` a partir do build STM32 e disponibilizar cppcheck para concluir a análise estática sem supressões novas.
- MÉDIO: a alteração que isola a falha de ADC fica fora de `USER CODE`; revisar o diff após toda regeneração CubeMX.
- HIL: executar a bancada documentada na NUCLEO-F446RE, incluindo falha controlada de ADC e continuidade dos demais subsistemas.

Não há evidência formal de conformidade MISRA.
