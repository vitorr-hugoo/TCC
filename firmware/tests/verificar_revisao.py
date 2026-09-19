"""Confere a presença do relatório manual obrigatório de revisão técnica."""

import argparse
import sys
from pathlib import Path

from _comum import CODIGO_NAO_CONFIGURADO, resolver


MARCADORES_OBRIGATORIOS = ("Resumo", "Escopo", "Evidências", "Limitações")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--raiz", type=Path, default=Path.cwd())
    parser.add_argument("--relatorio", type=Path, required=True)
    args = parser.parse_args()
    raiz = args.raiz.resolve()
    relatorio = resolver(raiz, args.relatorio)
    if not relatorio.is_file():
        print(f"Relatório de revisão inexistente: {relatorio}", file=sys.stderr)
        return CODIGO_NAO_CONFIGURADO
    texto = relatorio.read_text(encoding="utf-8")
    ausentes = [marcador for marcador in MARCADORES_OBRIGATORIOS if marcador not in texto]
    if ausentes:
        print("Relatório sem seções obrigatórias: " + ", ".join(ausentes), file=sys.stderr)
        return CODIGO_NAO_CONFIGURADO
    print(f"Relatório de revisão encontrado: {relatorio}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
