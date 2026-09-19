"""Compara warnings de builds STM32 atuais com o baseline versionado."""

import argparse
import json
import re
import sys
from pathlib import Path

from _comum import CODIGO_NAO_CONFIGURADO, resolver


PADRAO_WARNING = re.compile(
    r"(?P<arquivo>[^\r\n:]+):(?P<linha>\d+):(?P<coluna>\d+):\s+warning:\s+"
    r"(?P<mensagem>.*?)(?:\s+\[(?P<opcao>-[^\]]+)\])?\s*$"
)


def carregar_baseline(caminho: Path) -> set[tuple[str, int, str]]:
    dados = json.loads(caminho.read_text(encoding="utf-8-sig"))
    if not isinstance(dados, dict) or not isinstance(dados.get("warnings"), list):
        raise ValueError("Baseline deve conter uma lista warnings.")
    resultado = set()
    for item in dados["warnings"]:
        if not isinstance(item, dict):
            raise ValueError("Entrada inválida no baseline.")
        arquivo = item.get("arquivo")
        linha = item.get("linha")
        opcao = item.get("opcao")
        if not isinstance(arquivo, str) or not isinstance(linha, int) or not isinstance(opcao, str):
            raise ValueError("Baseline requer arquivo, linha e opcao.")
        resultado.add((arquivo.replace("\\", "/"), linha, opcao))
    return resultado


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--raiz", type=Path, default=Path.cwd())
    parser.add_argument("--diretorio-relatorios", type=Path, default=Path("../work/firmware/reports/build"))
    parser.add_argument("--baseline", type=Path, default=Path("tests/warnings_baseline.json"))
    args = parser.parse_args()
    raiz = args.raiz.resolve()
    diretorio_relatorios = resolver(raiz, args.diretorio_relatorios)
    logs = [diretorio_relatorios / f"{variante}.log" for variante in ("Debug", "Release")]
    if any(not log.is_file() for log in logs):
        print("Logs atuais de Debug e Release não encontrados; execute o build antes da avaliação.", file=sys.stderr)
        return CODIGO_NAO_CONFIGURADO
    try:
        baseline = carregar_baseline(resolver(raiz, args.baseline))
    except (OSError, ValueError, TypeError, json.JSONDecodeError) as erro:
        print(f"Baseline inválido: {erro}", file=sys.stderr)
        return CODIGO_NAO_CONFIGURADO

    encontrados = []
    for log in logs:
        for texto in log.read_text(encoding="utf-8", errors="replace").splitlines():
            correspondencia = PADRAO_WARNING.match(texto)
            if correspondencia is None:
                continue
            arquivo = correspondencia.group("arquivo").replace("\\", "/")
            linha = int(correspondencia.group("linha"))
            opcao = correspondencia.group("opcao") or ""
            chave = (arquivo, linha, opcao)
            encontrados.append((log.name, chave, correspondencia.group("mensagem")))

    novos = []
    for log, chave, mensagem in encontrados:
        classe = "BASELINE" if chave in baseline else "NOVO"
        print(f"{classe}: {log}: {chave[0]}:{chave[1]} {chave[2]} {mensagem}")
        if classe == "NOVO":
            novos.append(chave)
    print(f"Warnings encontrados: {len(encontrados)}; novos: {len(novos)}; baseline: {len(baseline)}")
    return 1 if novos else 0


if __name__ == "__main__":
    raise SystemExit(main())
