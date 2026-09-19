"""Executa as suítes unitárias host do STM32, LoRa e gateway."""

import sys

from _comum import criar_parser, executar, finalizar, validar_raiz


def main() -> int:
    parser = criar_parser(__doc__)
    args = parser.parse_args()
    validar_raiz(parser, args)
    comandos = [
        [sys.executable, "tests/sensors/run.py"],
        [sys.executable, "tests/lora/run.py"],
    ]
    resultados = [executar(comando, args.raiz, args.timeout) for comando in comandos]
    return finalizar(args, resultados)


if __name__ == "__main__":
    raise SystemExit(main())
