---
name: padrao-c-embarcado
description: Padroniza criação, alteração e revisão de código C para sistemas embarcados, seguindo as convenções deste projeto e práticas compatíveis com BARR-C e MISRA C. Use esta skill sempre que criar, modificar, refatorar ou revisar arquivos C, headers, drivers, módulos, firmware ou exemplos em C.
---

# Padrão C para Sistemas Embarcados

## Objetivo

Aplicar um padrão consistente, legível, seguro e adequado a sistemas embarcados durante a criação, alteração, refatoração e revisão de código C.

Esta Skill combina:

- preferências de estilo definidas pelo usuário;
- boas práticas inspiradas no BARR-C;
- práticas de segurança e robustez compatíveis com MISRA C;
- legibilidade e manutenibilidade para firmware embarcado.

Esta Skill **não declara nem certifica conformidade formal com MISRA C**. Conformidade MISRA exige definição da versão aplicável, análise das regras, registro de desvios, revisão e, normalmente, uso de ferramenta de análise estática apropriada.

## Prioridade das instruções

Ao trabalhar em um repositório:

1. respeite requisitos funcionais e de segurança do projeto;
2. respeite instruções específicas existentes em `AGENTS.md`;
3. respeite interfaces públicas, APIs externas e código gerado por ferramentas;
4. aplique esta Skill ao código criado ou alterado;
5. quando uma regra específica do projeto entrar em conflito com esta Skill, preserve a regra do projeto e informe a divergência quando relevante.

Não faça grandes reformatações em arquivos ou trechos não relacionados à tarefa atual.

---

# 1. Idioma do código

O código desenvolvido pelo projeto deve estar em **português**.

Use português para:

- nomes de variáveis;
- parâmetros;
- funções desenvolvidas no projeto;
- estruturas e tipos desenvolvidos no projeto;
- enumerações;
- comentários;
- documentação Doxygen;
- nomes de constantes internas quando não forem definidos por protocolo ou interface externa.

Exemplos:

```c
uint32_t tempoAmostragem = 1000U;
float temperaturaAtual = 0.0F;
bool sensorDisponivel = false;

void atualizarSensores(void);
uint16_t lerTensaoEntrada(void);
```

Não traduza nem renomeie:

- funções de bibliotecas externas;
- símbolos de HAL, SDK, RTOS ou CMSIS;
- nomes definidos por datasheets;
- registradores;
- protocolos;
- macros fornecidas pelo fabricante;
- APIs públicas externas;
- termos técnicos cuja tradução prejudique a clareza.

Exemplo:

```c
HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);
osDelay(10U);
```

---

# 2. Identação e chaves

## 2.1 Identação

Use **4 espaços por nível de identação**.

Nunca use tabulação para identar código.

Correto:

```c
if (sensorDisponivel)
{
    temperaturaAtual = lerTemperatura();

    if (temperaturaAtual > LIMITE_TEMPERATURA_C)
    {
        desligarAquecimento();
    }
}
```

## 2.2 Posição das chaves

Use o estilo **Allman**.

A chave de abertura deve ficar em uma nova linha, alinhada com a instrução que inicia o bloco.

Correto:

```c
if (estadoValido)
{
    processarEstado();
}
else
{
    registrarFalha();
}
```

Incorreto:

```c
if (estadoValido) {
    processarEstado();
} else {
    registrarFalha();
}
```

A mesma regra vale para:

- `if`;
- `else`;
- `for`;
- `while`;
- `do`;
- `switch`;
- funções;
- estruturas;
- uniões;
- enumerações.

---

# 3. Nomes de variáveis

Os nomes devem ser:

- intuitivos;
- descritivos;
- escritos em português;
- suficientemente claros para indicar sua finalidade;
- escritos em `lowerCamelCase`;
- sem `_` (underline).

Correto:

```c
uint16_t tensaoEntrada = 0U;
uint32_t contadorAmostras = 0U;
float temperaturaMedia = 0.0F;
bool comunicacaoAtiva = false;
```

Incorreto:

```c
uint16_t tensao_entrada;
uint32_t cnt;
float temp;
bool flag1;
```

Evite nomes genéricos como:

```c
valor
dado
aux
temp
x
y
var
flag
```

Esses nomes somente são aceitáveis quando o contexto matemático ou o escopo extremamente pequeno tornar seu significado inequívoco.

## 3.1 Parâmetros

Parâmetros de funções seguem a mesma regra das variáveis.

Correto:

```c
void configurarLimite(uint16_t limiteSuperior);
```

Incorreto:

```c
void configurarLimite(uint16_t limite_superior);
```

## 3.2 Funções

Funções desenvolvidas no projeto devem preferencialmente:

- usar português;
- usar `lowerCamelCase`;
- iniciar por um verbo que descreva a ação.

Exemplos:

```c
void atualizarSensores(void);
bool verificarComunicacao(void);
uint16_t calcularMediaAmostras(void);
```

## 3.3 Tipos

Tipos criados pelo projeto devem possuir nomes descritivos.

Evite sufixos reservados ou potencialmente conflitantes como `_t`.

Exemplo:

```c
typedef struct
{
    float temperatura;
    float umidade;
} DadosAmbiente;
```

---

# 4. Defines e macros

Defines e macros devem ser escritos totalmente em **MAIÚSCULAS**.

Use `_` para separar palavras em macros e defines.

Correto:

```c
#define TEMPO_AMOSTRAGEM_MS         (1000U)
#define TEMPO_TIMEOUT_UART_MS       (1000U)
#define MAXIMO_TENTATIVAS           (3U)
#define LIMITE_TEMPERATURA_C        (80.0F)
```

Sempre que houver vários `#define` relacionados, alinhe visualmente os valores.

## 4.1 Não reutilizar defines

Um define deve representar **uma única finalidade semântica**.

Não reutilize um define apenas porque duas configurações possuem atualmente o mesmo valor.

Incorreto:

```c
#define TEMPO_PADRAO_MS             (1000U)

osDelay(TEMPO_PADRAO_MS);
configurarTimeoutUart(TEMPO_PADRAO_MS);
```

Correto:

```c
#define TEMPO_AMOSTRAGEM_MS         (1000U)
#define TEMPO_TIMEOUT_UART_MS       (1000U)

osDelay(TEMPO_AMOSTRAGEM_MS);
configurarTimeoutUart(TEMPO_TIMEOUT_UART_MS);
```

Mesmo que os dois valores sejam iguais hoje, devem permanecer independentes quando representam conceitos diferentes.

## 4.2 Macros semelhantes a funções

Evite macros semelhantes a funções quando uma função `static inline` fornecer maior segurança de tipos e clareza.

Quando uma macro semelhante a função for realmente necessária:

- coloque parâmetros e expressão resultante entre parênteses;
- evite efeitos colaterais;
- não avalie um parâmetro mais de uma vez.

Evite:

```c
#define DOBRAR(x)                   ((x) + (x))
```

quando `x` puder conter uma expressão com efeito colateral.

Prefira uma função apropriada quando possível.

---

# 5. Inicialização de variáveis

Toda variável deve possuir um valor conhecido **antes do primeiro uso**.

Sempre que possível, inicialize a variável na declaração.

Correto:

```c
uint32_t contador = 0U;
uint16_t leituraAdc = 0U;
float tensao = 0.0F;
bool erroDetectado = false;
```

Para arrays:

```c
uint8_t bufferRecepcao[32] = {0U};
```

Para estruturas:

```c
DadosAmbiente dadosAmbiente = {0};
```

Quando o valor inicial vier imediatamente de uma função, também é aceitável:

```c
uint16_t leituraAdc = lerAdc();
```

Não deixe uma variável sem valor definido para atribuí-la apenas em alguns caminhos condicionais.

Incorreto:

```c
uint16_t leitura;

if (sensorDisponivel)
{
    leitura = lerSensor();
}

processarLeitura(leitura);
```

---

# 6. Tipos de dados

Em firmware embarcado, prefira tipos de largura explícita de `<stdint.h>` quando a largura for relevante.

Prefira:

```c
uint8_t
int8_t
uint16_t
int16_t
uint32_t
int32_t
uint64_t
int64_t
```

Use `bool` de `<stdbool.h>` para valores booleanos quando compatível com o projeto.

Evite escolher `int`, `long` ou `short` quando o código depender de uma largura específica.

Escolha o menor tipo que:

- comporte todo o intervalo esperado;
- não provoque overflow durante operações intermediárias;
- seja natural para a arquitetura e periférico envolvidos;
- não gere conversões desnecessárias.

Não reduza tipos apenas para economizar memória sem verificar o intervalo real.

---

# 7. Literais e constantes

Use sufixos adequados para deixar explícito o tipo dos literais quando relevante.

Exemplos:

```c
uint32_t contador = 0U;
uint32_t periodo = 1000UL;
float ganho = 1.5F;
```

Evite números mágicos espalhados pelo código.

Incorreto:

```c
if (temperatura > 80.0F)
{
    desligarSistema();
}
```

Prefira:

```c
#define LIMITE_TEMPERATURA_C        (80.0F)

if (temperatura > LIMITE_TEMPERATURA_C)
{
    desligarSistema();
}
```

Use constantes tipadas (`const`) ou enumerações quando forem mais adequadas que macros.

---

# 8. Conversões de tipos

Evite conversões implícitas que possam:

- perder precisão;
- alterar sinal;
- truncar valores;
- causar overflow;
- gerar comportamento dependente da implementação.

Tenha atenção especial ao misturar:

- signed e unsigned;
- inteiros de larguras diferentes;
- inteiros e ponto flutuante;
- enums e inteiros;
- ponteiros de tipos diferentes.

Quando uma conversão for necessária, faça-a de forma explícita e somente após garantir que o valor cabe no tipo de destino.

Exemplo:

```c
if (valorAdc <= UINT16_MAX)
{
    leituraAdc = (uint16_t)valorAdc;
}
```

Não adicione casts apenas para silenciar warnings sem entender a causa.

---

# 9. Expressões

Use parênteses para deixar clara a precedência em expressões compostas.

Prefira:

```c
resultado = (ganho * entrada) + deslocamento;
```

em vez de depender desnecessariamente da precedência implícita.

Não use atribuições dentro de condições.

Evite:

```c
if ((estado = lerEstado()) != ESTADO_ERRO)
```

Prefira:

```c
estado = lerEstado();

if (estado != ESTADO_ERRO)
{
    processarEstado(estado);
}
```

Evite múltiplos efeitos colaterais na mesma expressão.

---

# 10. Estruturas condicionais

Todas as estruturas condicionais devem usar chaves, mesmo quando possuem apenas uma instrução.

Correto:

```c
if (erroDetectado)
{
    registrarErro();
}
```

Incorreto:

```c
if (erroDetectado)
    registrarErro();
```

## 10.1 Switch

Todo `switch` deve possuir tratamento explícito dos casos esperados.

Inclua `default` quando adequado ao projeto, principalmente para detectar estados inesperados.

Exemplo:

```c
switch (estadoSistema)
{
    case ESTADO_INICIALIZANDO:
    {
        inicializarSistema();
        break;
    }

    case ESTADO_EXECUTANDO:
    {
        executarSistema();
        break;
    }

    case ESTADO_ERRO:
    {
        tratarErro();
        break;
    }

    default:
    {
        tratarEstadoInvalido();
        break;
    }
}
```

Não permita `fallthrough` implícito. Se um `fallthrough` for intencional e aceito pelas regras do projeto, documente-o claramente.

---

# 11. Funções

Cada função deve possuir responsabilidade clara e limitada.

Prefira funções:

- pequenas;
- coesas;
- com poucos efeitos colaterais;
- com nomes que expressem a ação executada;
- com número reduzido de parâmetros;
- com interfaces previsíveis.

Evite funções extensas que misturem:

- aquisição;
- processamento;
- controle;
- comunicação;
- armazenamento;
- tratamento de erro.

Separe responsabilidades quando isso melhorar a compreensão e testabilidade.

Use `static` em funções que não precisam ser visíveis fora do arquivo.

Exemplo:

```c
static uint16_t calcularMedia(const uint16_t *amostras, uint16_t quantidade);
```

---

# 12. Parâmetros e ponteiros

Use `const` sempre que uma função não precisar modificar o dado apontado.

Exemplo:

```c
uint16_t calcularMedia(const uint16_t *amostras, uint16_t quantidade);
```

Antes de desreferenciar ponteiros que possam ser inválidos, valide-os quando essa condição puder ocorrer.

Exemplo:

```c
if (dados == NULL)
{
    return RESULTADO_PARAMETRO_INVALIDO;
}
```

Evite aritmética de ponteiros quando indexação de arrays fornecer uma solução mais clara e segura.

Não use casts entre tipos de ponteiros sem necessidade técnica clara e justificável.

---

# 13. Variáveis globais

Evite variáveis globais mutáveis.

Quando o estado precisar ser restrito a um módulo, prefira:

```c
static uint32_t contadorEventos = 0U;
```

Use interfaces de acesso quando isso melhorar encapsulamento.

Variáveis compartilhadas entre:

- interrupções;
- tarefas;
- threads;
- DMA;
- hardware;

devem receber tratamento apropriado de concorrência e visibilidade.

`volatile` não substitui mecanismos de sincronização nem garante atomicidade.

---

# 14. Memória dinâmica

Em firmware embarcado, não introduza:

```c
malloc()
calloc()
realloc()
free()
```

sem que o projeto solicite explicitamente ou já possua uma política clara para memória dinâmica.

Prefira alocação estática quando possível.

Considere:

- fragmentação;
- determinismo;
- falha de alocação;
- tempo de execução;
- consumo de memória.

---

# 15. Recursão e fluxo de controle

Evite recursão em firmware embarcado, principalmente quando houver restrições de stack ou requisitos de determinismo.

Evite `goto` em código novo, salvo quando houver uma justificativa técnica clara, um padrão já estabelecido no projeto ou necessidade específica de tratamento seguro de recursos.

Não crie fluxos de controle desnecessariamente complexos.

---

# 16. Retornos e erros

Valores de retorno de funções que podem falhar devem ser verificados.

Incorreto:

```c
inicializarSensor();
```

quando a função informa sucesso ou falha.

Prefira:

```c
resultado = inicializarSensor();

if (resultado != RESULTADO_OK)
{
    tratarFalhaSensor(resultado);
}
```

Não ignore erros silenciosamente.

Quando um retorno for intencionalmente ignorado e isso for permitido pelo projeto, deixe a intenção explícita.

---

# 17. Doxygen

Comentários de documentação devem utilizar **Doxygen** e ser escritos em português.

Documente pelo menos:

- interfaces públicas;
- funções não triviais;
- estruturas públicas;
- enumerações públicas;
- parâmetros cujo propósito não seja imediatamente óbvio;
- valores de retorno;
- condições de erro;
- unidades de medida;
- pré-condições importantes;
- efeitos colaterais relevantes.

## 17.1 Cabeçalho de arquivo

Exemplo:

```c
/**
 * @file sensorTemperatura.c
 * @brief Implementação do módulo de aquisição de temperatura.
 *
 * Responsável pela inicialização, leitura e validação dos dados
 * provenientes do sensor de temperatura.
 */
```

## 17.2 Funções

Exemplo:

```c
/**
 * @brief Realiza a leitura da temperatura do sensor.
 *
 * @param identificadorSensor Identificador do sensor que será lido.
 *
 * @return Temperatura medida em graus Celsius.
 */
float lerTemperatura(uint8_t identificadorSensor);
```

## 17.3 Função com código de erro

```c
/**
 * @brief Inicializa o módulo de comunicação.
 *
 * @param velocidadeBaud Velocidade da interface UART em bit/s.
 *
 * @retval RESULTADO_OK Inicialização realizada com sucesso.
 * @retval RESULTADO_ERRO Falha durante a inicialização.
 */
Resultado inicializarComunicacao(uint32_t velocidadeBaud);
```

## 17.4 Comentários internos

Comentários internos devem explicar **por que** uma decisão existe, e não simplesmente repetir o código.

Ruim:

```c
/* Incrementa o contador. */
contador++;
```

Melhor:

```c
/* Mantém a contagem para diagnóstico de perdas entre ciclos de aquisição. */
contadorEventos++;
```

Não polua o código com comentários óbvios.

---

# 18. Arquivos de cabeçalho

Headers devem possuir proteção contra inclusão múltipla.

Exemplo:

```c
#ifndef SENSOR_TEMPERATURA_H
#define SENSOR_TEMPERATURA_H

/* Conteúdo do header. */

#endif /* SENSOR_TEMPERATURA_H */
```

Inclua no header somente dependências realmente necessárias.

Não exponha detalhes internos de implementação sem necessidade.

---

# 19. Ordem recomendada de um arquivo C

Quando a estrutura existente do projeto não definir outra ordem, prefira:

```text
1. Comentário Doxygen do arquivo
2. Includes
3. Defines e macros
4. Tipos privados
5. Variáveis estáticas do módulo
6. Protótipos de funções privadas
7. Funções públicas
8. Funções privadas
```

Mantenha itens relacionados próximos uns dos outros.

---

# 20. Includes

Inclua somente o necessário.

Não dependa intencionalmente de includes transitivos.

Prefira uma organização consistente:

```c
#include "sensorTemperatura.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "adc.h"
#include "diagnostico.h"
```

Quando o projeto já possuir convenção diferente, preserve a convenção existente.

---

# 21. Código gerado e bibliotecas externas

Não reformate, renomeie ou altere código de terceiros apenas para adequá-lo a esta Skill.

Isso inclui:

- HAL;
- CMSIS;
- FreeRTOS;
- SDKs;
- bibliotecas externas;
- código gerado automaticamente.

Quando código gerado possuir regiões explicitamente destinadas ao usuário, mantenha alterações dentro dessas regiões.

Preserve nomes e assinaturas exigidos por APIs externas.

---

# 22. Boas práticas compatíveis com MISRA C

Ao criar ou revisar código, procure reduzir construções propensas a erro.

Em particular:

- inicialize variáveis antes do primeiro uso;
- use tipos adequados e de largura conhecida;
- evite conversões implícitas perigosas;
- controle operações signed/unsigned;
- evite overflow e underflow;
- valide índices antes de acessar arrays;
- valide ponteiros quando aplicável;
- verifique retornos de funções;
- evite comportamento indefinido;
- evite dependência de comportamento não especificado;
- minimize efeitos colaterais em expressões;
- use `const` quando apropriado;
- restrinja símbolos ao menor escopo possível;
- use `static` para implementação interna;
- não introduza código inalcançável;
- não mantenha variáveis ou funções não utilizadas;
- não suprima warnings sem investigar sua causa;
- trate todos os estados relevantes de máquinas de estado;
- prefira soluções determinísticas.

Se o projeto possuir ferramenta de análise MISRA configurada, respeite seus diagnósticos e não adicione supressões ou desvios sem justificativa explícita.

---

# 23. Segurança em operações aritméticas

Antes de operações que possam exceder o tipo, considere o intervalo intermediário.

Exemplo problemático:

```c
uint16_t valorA = 50000U;
uint16_t valorB = 2U;
uint16_t resultado = valorA * valorB;
```

Considere promover explicitamente a operação para largura suficiente quando necessário:

```c
uint16_t valorA = 50000U;
uint16_t valorB = 2U;
uint32_t resultado = (uint32_t)valorA * (uint32_t)valorB;
```

A escolha do tipo deve considerar não apenas o resultado final esperado, mas também os resultados intermediários.

---

# 24. Concorrência, interrupções e RTOS

Ao trabalhar com firmware concorrente:

- mantenha ISRs curtas;
- não execute operações bloqueantes dentro de interrupções;
- não assuma que `volatile` fornece sincronização;
- proteja dados compartilhados quando necessário;
- considere atomicidade das operações;
- utilize primitivas adequadas do RTOS quando existentes;
- documente claramente dados compartilhados entre contexto de interrupção e tarefas.

Não altere prioridades, seções críticas, mutexes, semáforos ou mecanismos de sincronização sem avaliar impactos de tempo real.

---

# 25. Alterações em código existente

Ao modificar um arquivo:

1. entenda primeiro o padrão local e a arquitetura;
2. preserve comportamento não relacionado à tarefa;
3. aplique esta Skill ao código novo e ao trecho diretamente alterado;
4. não reformate o arquivo inteiro sem necessidade;
5. não renomeie APIs públicas sem solicitação;
6. não mude tipos ou interfaces apenas por preferência estética;
7. investigue warnings novos;
8. mantenha alterações pequenas e rastreáveis.

---

# 26. Processo obrigatório antes de finalizar

Antes de considerar uma alteração concluída, revise:

## Formatação

- [ ] A identação usa exatamente 4 espaços.
- [ ] Não foram introduzidos tabs.
- [ ] As chaves seguem o estilo Allman.
- [ ] Estruturas condicionais usam chaves mesmo com uma única instrução.

## Idioma e nomes

- [ ] Código desenvolvido no projeto está em português.
- [ ] Variáveis possuem nomes intuitivos.
- [ ] Variáveis usam `lowerCamelCase`.
- [ ] Variáveis não possuem underline.
- [ ] Funções novas possuem nomes claros e orientados a ação.
- [ ] Identificadores externos não foram traduzidos ou renomeados indevidamente.

## Defines e constantes

- [ ] Defines/macros estão em MAIÚSCULAS.
- [ ] Defines relacionados estão alinhados.
- [ ] Cada define possui uma única finalidade semântica.
- [ ] O mesmo define não foi reutilizado para configurações conceitualmente diferentes.
- [ ] Não foram introduzidos números mágicos desnecessários.

## Variáveis e tipos

- [ ] Todas as variáveis possuem valor definido antes do primeiro uso.
- [ ] O tipo escolhido comporta todo o intervalo necessário.
- [ ] Operações intermediárias não provocam overflow.
- [ ] Conversões signed/unsigned foram verificadas.
- [ ] Casts possuem justificativa técnica.

## Segurança

- [ ] Índices de arrays estão dentro dos limites.
- [ ] Ponteiros potencialmente inválidos são tratados.
- [ ] Valores de retorno relevantes são verificados.
- [ ] Não há comportamento indefinido evidente.
- [ ] Não foi introduzida memória dinâmica sem justificativa.
- [ ] Não foi introduzida recursão sem justificativa.

## Documentação

- [ ] Interfaces e funções relevantes possuem documentação Doxygen.
- [ ] Comentários estão em português.
- [ ] Comentários explicam intenção e decisões, não apenas repetem o código.
- [ ] Unidades de medida estão documentadas quando relevantes.

## Integração

- [ ] Código externo ou gerado não foi alterado desnecessariamente.
- [ ] APIs existentes foram preservadas quando possível.
- [ ] Build foi executado, quando disponível.
- [ ] Testes foram executados, quando disponíveis.
- [ ] Análise estática foi executada, quando configurada.
- [ ] Nenhum warning novo foi ignorado sem análise.

---

# 27. Exemplo completo

```c
/**
 * @file monitorTemperatura.c
 * @brief Implementação do monitoramento de temperatura.
 */

#include "monitorTemperatura.h"

#include <stdbool.h>
#include <stdint.h>

#define TEMPO_AMOSTRAGEM_MS         (1000U)
#define LIMITE_TEMPERATURA_C        (80.0F)
#define MAXIMO_FALHAS_CONSECUTIVAS  (3U)

static uint32_t contadorFalhas = 0U;

/**
 * @brief Verifica se a temperatura ultrapassou o limite permitido.
 *
 * @param temperatura Temperatura atual em graus Celsius.
 *
 * @return `true` quando o limite for excedido; caso contrário, `false`.
 */
static bool verificarSobretemperatura(float temperatura)
{
    bool limiteExcedido = false;

    if (temperatura > LIMITE_TEMPERATURA_C)
    {
        limiteExcedido = true;
    }

    return limiteExcedido;
}

/**
 * @brief Executa um ciclo de monitoramento da temperatura.
 *
 * Realiza a aquisição do sensor e trata uma possível condição
 * de sobretemperatura.
 */
void executarMonitoramentoTemperatura(void)
{
    float temperaturaAtual = 0.0F;
    ResultadoLeitura resultadoLeitura = RESULTADO_LEITURA_ERRO;

    resultadoLeitura = lerTemperatura(&temperaturaAtual);

    if (resultadoLeitura == RESULTADO_LEITURA_OK)
    {
        contadorFalhas = 0U;

        if (verificarSobretemperatura(temperaturaAtual))
        {
            desligarSaidaPotencia();
            registrarSobretemperatura(temperaturaAtual);
        }
    }
    else
    {
        contadorFalhas++;

        if (contadorFalhas >= MAXIMO_FALHAS_CONSECUTIVAS)
        {
            registrarFalhaSensor();
        }
    }
}
```

---

# 28. Resultado esperado do Codex

Ao aplicar esta Skill, o Codex deve:

- produzir código consistente com estas regras;
- preservar a arquitetura existente;
- priorizar clareza e segurança;
- evitar mudanças cosméticas fora do escopo;
- corrigir violações encontradas diretamente no código alterado quando isso puder ser feito sem modificar o comportamento;
- informar violações relevantes que não possam ser corrigidas com segurança;
- não afirmar conformidade formal com MISRA C apenas porque esta Skill foi aplicada.

Quando houver dúvida entre código mais curto e código mais explícito, prefira a solução mais clara, segura e verificável para sistemas embarcados.
