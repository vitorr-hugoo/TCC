"""Executa gates de um projeto; não concede aprovação técnica do firmware."""

import json
import math
import sys
from pathlib import Path

from _comum import criar_parser, executar, gravar_relatorio, resolver, validar_raiz


ETAPAS = ("formatacao", "build", "warnings", "analise_estatica", "testes_unitarios",
          "testes_automatizados", "hil", "code_review")


def ler_configuracao(caminho: Path) -> dict:
    dados = json.loads(caminho.read_text(encoding="utf-8-sig"))
    if not isinstance(dados, dict) or set(dados) != {"revisao", "etapas"}:
        raise ValueError("Configuração deve conter somente revisao e etapas.")
    if not isinstance(dados["revisao"], str) or not dados["revisao"].strip():
        raise ValueError("Identifique revisão/diff e variante em revisao.")
    etapas = dados["etapas"]
    if not isinstance(etapas, dict) or set(etapas) != set(ETAPAS):
        raise ValueError(f"Declare todas e somente estas etapas: {', '.join(ETAPAS)}")
    for nome, etapa in etapas.items():
        obrigatorios = {"obrigatoria", "comando", "depende_de", "motivo"}
        if not isinstance(etapa, dict) or not obrigatorios <= set(etapa) or set(etapa) - obrigatorios - {"timeout"}:
            raise ValueError(f"Campos inválidos em {nome}.")
        if not isinstance(etapa["obrigatoria"], bool):
            raise ValueError(f"obrigatoria deve ser booleano em {nome}.")
        comando = etapa["comando"]
        if comando is not None and (not isinstance(comando, list) or not comando or
                                   not all(isinstance(a, str) and a for a in comando)):
            raise ValueError(f"comando deve ser null ou lista não vazia de strings em {nome}.")
        if not isinstance(etapa["motivo"], str) or (comando is None and not etapa["motivo"].strip()):
            raise ValueError(f"Etapa sem comando exige motivo em {nome}.")
        dependencias = etapa["depende_de"]
        if not isinstance(dependencias, list) or any(d not in ETAPAS[:ETAPAS.index(nome)] for d in dependencias):
            raise ValueError(f"Dependências de {nome} devem ser etapas anteriores; ciclos não permitidos.")
        prazo = etapa.get("timeout", 300)
        if isinstance(prazo, bool) or not isinstance(prazo, (int, float)) or not math.isfinite(prazo) or prazo <= 0:
            raise ValueError(f"Timeout inválido em {nome}.")
    return dados


def main() -> int:
    parser = criar_parser(__doc__)
    parser.add_argument("--config", type=Path, required=True)
    args = parser.parse_args()
    validar_raiz(parser, args)
    try:
        dados = ler_configuracao(resolver(args.raiz, args.config))
        resultados = {}
        for nome in ETAPAS:
            etapa = dados["etapas"][nome]
            impedimentos = [dep for dep in etapa["depende_de"] if resultados[dep]["status"] != "PASS"]
            if impedimentos:
                resultado = {"status": "NÃO EXECUTADO", "motivo": f"Dependências sem PASS: {impedimentos}"}
            elif etapa["comando"] is None:
                resultado = {"status": "NÃO CONFIGURADO", "motivo": etapa["motivo"]}
            else:
                comando = [sys.executable if item == "{python}" else item for item in etapa["comando"]]
                resultado = executar(comando, args.raiz, etapa.get("timeout", args.timeout))
            resultado["obrigatoria"] = etapa["obrigatoria"]
            resultados[nome] = resultado
            print(f"{nome}: {resultado['status']}")
        falha = any(r["obrigatoria"] and r["status"] in {"FAIL", "TIMEOUT"} for r in resultados.values())
        pendencia = any(r["obrigatoria"] and r["status"] != "PASS" for r in resultados.values())
        codigo = 1 if falha else (2 if pendencia else 0)
        relatorio = {
            "revisao_declarada": dados["revisao"],
            "resultado_automatico": "FALHA" if falha else ("INCOMPLETO" if pendencia else "GATES OBRIGATÓRIOS PASSARAM"),
            "aprovacao_tecnica": "NÃO AVALIADA — exige revisão semântica e validacao-firmware",
            "ressalvas": [nome for nome, r in resultados.items() if r["status"] != "PASS"],
            "etapas": resultados,
        }
        gravar_relatorio(resolver(args.raiz, args.relatorio or Path("relatorios/validacao.json")), relatorio)
        print(relatorio["resultado_automatico"])
        return codigo
    except (OSError, ValueError, TypeError) as erro:
        print(f"Erro de configuração/relatório: {erro}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
