# Diagnóstico contínuo dos sensores — implementação local

Data: 11/09/2026. Branch: `desenvolvimento/v0.9`.

Implementadas as etapas de diagnóstico local do [plano 003](../plans/003-diagnostico-continuo-dos-sensores.md). Sem ACK, sem mudança do pacote LoRa de 18 bytes, sem alteração do esquema de `data.csv`. Não houve commit, push nem gravação na placa nesta etapa. As alterações anteriores de LoRa foram preservadas.

## Comportamento entregue

- Uma falha invalida imediatamente a leitura daquele sensor. Os demais sensores continuam, com SD e tentativa de transmissão ao fechar o ciclo.
- Cada sensor possui validade, motivo, número saturado de falhas consecutivas, instante da última leitura válida e indicação de que já houve uma leitura válida. O estado inicial ainda não foi avaliado.
- Logs de diagnóstico registram primeira leitura, mudança de motivo/estado, terceira falha consecutiva e recuperação. Não repetem o mesmo alerta em toda iteração. Uma leitura válida encerra a sequência de falhas.
- A coleta forma uma amostra única para serial, SD e codificação LoRa. Resultados não finitos/fora da faixa são invalidados antes das conversões. O último valor bom permanece no cache do driver, mas não é publicado como amostra nova.
- O ciclo mantém a espera nominal de 5 segundos entre aquisições. As leituras continuam sequenciais; não foram criadas tarefas, RTOS ou recuperação por reset generalizado.

## Alterações por sensor

| Componente | Implementação |
|---|---|
| DHT22 | Esperas medidas pelo TIM10 em microssegundos, limite de borda de 120 µs e escape independente para timer parado. Verifica duração do pulso, checksum, temperatura de −40 a 80 °C e umidade de 0 a 100%. Só atualiza temperatura/umidade juntas após validar o quadro. Boot informa interface configurada, sem afirmar presença física. |
| BMP280 | Verifica chip ID e configuração antes/depois da leitura; valida calibração na inicialização. Aguarda medição/NVM com prazo calculado do oversampling (49 ms na configuração atual, incluindo margem). Rejeita raw desabilitado, divisor zero, valores não finitos e pressão/temperatura fora das faixas. Compensação usa a fórmula Bosch em double para evitar operações inteiras com overflow/deslocamento de sinal. Mantém modo normal na aplicação. |
| BH1750 | Aplicação usa medição única H-Resolution: Power On, comando e espera de 180 ms em cada ciclo. Verifica TX/RX I2C. Zero lux é válido; raw 65535 indica saturação e invalida também a irradiância derivada. Não inventa ID ou checksum. |
| Biruta | Mantém a LUT existente. Exige maioria das amostras mapeadas: 5 de 8. Erro de inicialização/ADC invalida apenas direção, incluindo falhas de `MX_ADC1_Init`. Preserva saída em falha de leitura e desliga clock do ADC nos caminhos de erro. |
| Anemômetro | Confere início/parada do TIM6; serviço de timeout em 1000 ms + margem configurável de 1000 ms. Limpa flags e IRQ pendentes e ignora callback de janela cancelada/cedo demais. Preserva PRIMASK. Não reaproveita resultado antigo ao falhar. Expõe pulsos aceitos/rejeitados e instante do último pulso; mantém debounce e filtro inicial existentes. Zero pulsos em janela concluída é válido. |
| Pluviômetro | Snapshot atômico com PRIMASK preservado, contagem cumulativa contínua e delta entre leituras. Registra pulsos rejeitados e último evento. Não zera por falha de outro sensor. Se exceder o contador uint32, satura e invalida a leitura em vez de retornar a zero. Ausência de chuva é válida. |

Falhas transitórias permitem nova leitura no ciclo seguinte. Inicialização incompleta, calibração inválida ou perda de configuração do BMP280 exigem inicialização validada — nesta versão, corrigir a causa e reiniciar a estação. Não há laço automático ilimitado de reset/reinit.

O timeout antigo do anemômetro já existia no final do loop; ele descartava a aquisição sem salvar/transmitir. Agora timeout e erro de timer passam pelo mesmo fechamento de ciclo usado no sucesso.

## Serial e arquivos no SD

`/logs/data.csv` mantém cabeçalho, ordem das colunas e separador ponto e vírgula. Campo inválido usa `-999`; chuva válida mantém o total completo, inclusive acima da capacidade do pacote.

Novo `/logs/sensor_health.csv`, somente para eventos:

```text
timestamp_ms;sensor;evento;valido;motivo;falhas;tem_ultima_valida;ultima_valida_ms;idade_ms
```

Quando `tem_ultima_valida=0`, os campos de instante/idade iguais a zero não representam uma leitura válida. Os instantes usam o relógio relativo da sessão e reiniciam no boot. Subtrações de tempo suportam o rollover de uint32; idade não deve ser usada para intervalos superiores a uma volta desse relógio.

Falha ao abrir, escrever integralmente ou fechar o diagnóstico leva o SD ao estado de falha já existente, com recuperação entre ciclos. A estação continua. Eventos ocorridos sem cartão são emitidos na serial; não há fila para gravá-los retroativamente ao reconectar o SD.

## Limitação do pacote legado

O tamanho continua **18 bytes**, com mesmo layout e CRC. DHT inválido usa temperatura −9990 e umidade zero; BMP/BH inválidos usam zero; vento inválido usa −9990; direção inválida usa 999; chuva inválida/sem representação usa zero.

As escalas de temperatura, umidade, vento e chuva são arredondadas para a unidade do pacote antes da conversão. Pressão e lux mantêm a conversão para unidades inteiras do formato anterior. Todas as conversões são precedidas de validação.

Chuva em centésimos de mm só cabe até 65535 unidades (655,35 mm). Ao exceder a representação arredondada, o total local continua correto, o campo transmitido usa zero e a serial registra `[LORA] Limite do pacote legado`. O mesmo tratamento impede overflow de velocidade. A máscara desse aviso é local: bit 4 = velocidade, bit 5 = chuva. O aviso ocorre quando essa máscara muda.

**Zero remoto continua ambíguo:** o gateway de 18 bytes não distingue zero legítimo de campo substituído por falha/limite. O diagnóstico completo é local, na serial/SD. Não interpretar esse zero como prova de escuridão ou de chuva acumulada zero. A etapa opcional de 19 bytes com máscara de validade permanece separada.

## Validação automatizada

Execução e detalhes em [tests/sensors/README.md](../../../tests/sensors/README.md).

- Suíte de sensores: drivers reais com HAL simulado; inclui forma de onda DHT, vetor de referência BMP, saturação BH, ADC, timers, debounce, relógios, caches e limites de codificação.
- Integração: executa trechos extraídos diretamente do `main.c`, com drivers e armazenamento reais, conferindo que os demais dados são salvos/transmitidos mesmo com falhas individuais e com SD indisponível.
- Regressão LoRa: 50 cenários anteriores mantidos.
- Builds ARM Debug e Release no STM32CubeIDE, sem gravar hardware.

Resultados: **75/75** cenários de sensores/integração e **50/50** de LoRa aprovados. Debug e Release concluídos sem erros. A compilação de FatFs ainda emite o aviso preexistente `-Wmisleading-indentation` em `ff.c:5032`; nenhum aviso novo nos arquivos desta implementação.

| Build | text | data | bss |
|---|---:|---:|---:|
| Debug | 82620 | 504 | 6384 |
| Release | 48000 | 140 | 6388 |

O objeto FatFs do diagnóstico usa armazenamento estático, como o arquivo principal, para não carregar seu buffer de setor na pilha. O uso de pilha próprio de `Storage_AppendSensorHealth` em Debug é 288 bytes, conforme o relatório `.su` do compilador.

Comando de build usado: STM32CubeIDE headless, projeto `Firmware_V0.8`, configurações Debug e Release, com workspace temporário. Os arquivos gerados permanecem nas pastas ignoradas de build. A alteração automática do hash de ambiente da IDE foi revertida para não misturar configuração local com o firmware.

## Validação pendente na bancada

1. Registrar vários ciclos normais na serial e conferir ambos os CSVs; manter calmaria, escuridão, tempo seco e direção fixa para conferir ausência de alarmes falsos.
2. Interromper de forma controlada a conexão de um sensor por vez. Conferir invalidade imediata, alerta no terceiro ciclo e continuidade dos outros dados. Restabelecer a conexão e verificar recuperação quando a configuração do dispositivo estiver preservada.
3. DHT22: conferir pulsos e duração da leitura em Debug/Release; BMP280/BH1750: verificar desconexão individual e falha comum do I2C1. BH1750: testar escuro e saturação por luz intensa.
4. Biruta: usar `Wind_Vane_ReadRaw` para registrar códigos em todas as posições, transições, cabo aberto e condições elétricas controladas. A LUT atual cobre 0–4095, portanto cabo aberto pode parecer uma direção válida. Nenhuma nova faixa de desconexão foi inventada; eventual distinção depende dessas medições ou de alteração elétrica.
5. Girar o anemômetro e provocar basculadas conhecidas no pluviômetro, conferindo pulsos e debounce. Sem estímulo físico, ausência de pulsos não demonstra cabo aberto ou mecanismo travado.
6. Simular falha/atraso da janela do TIM6 e retirar o SD durante operação; verificar continuidade e retomada. Medir o pior tempo de leitura para confirmar a margem de timeout em hardware.

As duas saídas de erro em `MX_ADC1_Init` foram alteradas para isolar falhas do ADC. Esses trechos ficam fora das regiões preservadas pelo CubeMX: revisar o diff se regenerar `main.c` a partir do `.ioc`, para não reintroduzir `Error_Handler` nesses caminhos.
