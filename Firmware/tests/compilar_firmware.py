"""Executa os builds STM32 e ESP32 configurados, mantendo falhas independentes visíveis."""

import argparse
import subprocess
import sys
from pathlib import Path

from _comum import CODIGO_NAO_CONFIGURADO, CODIGO_NAO_DISPONIVEL


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--raiz", type=Path, default=Path.cwd())
    parser.add_argument("--timeout", type=float, default=300.0)
    args = parser.parse_args()
    raiz = args.raiz.resolve()
    if not raiz.is_dir():
        parser.error(f"Raiz inexistente: {raiz}")
    if args.timeout <= 0:
        parser.error("--timeout deve ser positivo.")

    comandos = (
        ("STM32", [sys.executable, "tests/compilar_stm32.py", "--raiz", str(raiz), "--timeout", str(args.timeout)]),
        ("gateway ESP32", [sys.executable, "tests/compilar_gateway.py", "--raiz", str(raiz), "--timeout", str(args.timeout)]),
    )
    codigos = []
    for nome, comando in comandos:
        resultado = subprocess.run(comando, cwd=raiz, check=False)
        codigos.append(resultado.returncode)
        print(f"{nome}: exit {resultado.returncode}")

    if any(codigo not in {0, CODIGO_NAO_CONFIGURADO, CODIGO_NAO_DISPONIVEL} for codigo in codigos):
        return 1
    if any(codigo == CODIGO_NAO_DISPONIVEL for codigo in codigos):
        return CODIGO_NAO_DISPONIVEL
    if any(codigo == CODIGO_NAO_CONFIGURADO for codigo in codigos):
        return CODIGO_NAO_CONFIGURADO
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
