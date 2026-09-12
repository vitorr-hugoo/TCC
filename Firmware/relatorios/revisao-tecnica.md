# Revisão técnica de firmware

Revisão: worktree local em 12/09/2026, após a migração de estilo, a configuração do build STM32 e o isolamento de credenciais do gateway ESP32.

## Resumo

Críticos: 0
Altos: 1
Médios: 2
Baixos: 0
Estilo: 0

## [ALTO] Credencial de rede previamente versionada deve ser rotacionada

Arquivo: `Gateway ESP32/gateway_esp32_lora_V1.2.ino` e `Gateway ESP32/gateway_credentials.example.h`.

Problema: o sketch continha uma credencial Wi-Fi em texto aberto. A revisão atual moveu as macros de conexão para `gateway_credentials.h`, arquivo local ignorado pelo Git, e adicionou um modelo sem valores reais. O histórico e quaisquer clones anteriores podem ainda conter a credencial exposta.

Consequência: quem obteve uma revisão anterior pode tentar acesso à rede associada à credencial. A mudança no worktree evita novas versões do firmware com esse valor, mas não revoga o valor já exposto.

Correção recomendada: rotacionar a credencial no ambiente de rede e atualizar apenas o cabeçalho local ignorado. Caso o repositório tenha sido compartilhado, executar procedimento controlado de remoção de segredo do histórico e invalidar clones ou cópias distribuídas.

## [MÉDIO] A correção de inicialização do ADC fica fora de USER CODE

Arquivo: `STM32/Core/Src/main.c`, funções de inicialização de ADC.

Problema: os retornos de erro de `HAL_ADC_Init` e `HAL_ADC_ConfigChannel` isolam a falha da biruta, mas pertencem ao corpo gerado pelo CubeMX, fora dos marcadores `USER CODE`.

Consequência: uma regeneração pode restaurar `Error_Handler` e voltar a interromper aquisição, armazenamento e transmissão quando o ADC falhar.

Correção recomendada: revisar esse diff após cada regeneração do IOC e executar novamente os casos de falha de inicialização do ADC, build ARM e ensaio na NUCLEO-F446RE.

## [MÉDIO] Build limpo do gateway não conclui de forma reprodutível

Arquivo: `tests/compilar_gateway.py` e `relatorios/build/gateway.log`.

Problema: após limpar a saída Arduino antes de cada build, Arduino CLI 1.2.0 com ESP32 Core 2.0.17 não confirmou conclusão em 300 s, tanto com um quanto com quatro jobs. O runner encerra a árvore, remove a saída parcial e não reutiliza binários anteriores.

Consequência: o binário ESP32 não está validado para o worktree final. Uma execução anterior imprimiu o tamanho do sketch, mas não pode servir como evidência depois da alteração do runner e das execuções limpas que falharam.

Correção recomendada: investigar a instalação Arduino/ESP32 em ambiente limpo, incluindo bloqueios de arquivos e processos filhos, até que um build limpo conclua com retorno zero. Só então reexecutar warnings e análise pertinentes ao gateway.

## Escopo

Foram revisados os módulos próprios ativos, os scripts de build e validação, o IOC, os makefiles gerados, a configuração de formatação, a configuração clang-tidy e o caminho de inicialização do ADC. A migração preserva APIs públicas, protocolo LoRa, HAL/CMSIS, código CubeMX e bibliotecas externas.

## Evidências

- `python tests/verificar_formatacao.py --raiz . --proprios` aprovou 45 fontes próprias com clang-format 14.0.0.
- `python tests/compilar_stm32.py --raiz . --timeout 90` aprovou Debug e Release com GNU Make 4.4.1 e ARM GCC 14.3.1.
- `python tests/avaliar_warnings.py --raiz .` encontrou 0 warnings e 0 novos nos logs STM32 atuais.
- Os testes host aprovaram 78/78 cenários de sensores/integração, 54/54 de LoRa/gateway e o cenário automatizado `radio_ff`.
- `clang-tidy --verify-config` aprovou a configuração. A análise completa não executou porque não existe `compile_commands.json` gerado pelo build STM32; cppcheck também não está disponível.

## Limitações

Não houve HIL: não há placa, porta serial, procedimento de gravação ou bancada identificados para esta execução. O teste host não demonstra comportamento elétrico, temporização real, RF, memória de pior caso ou publicação MQTT. Não há evidência formal de conformidade MISRA.
