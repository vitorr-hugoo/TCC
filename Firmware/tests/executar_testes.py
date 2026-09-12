"""Envolve o runner existente; a contagem de testes pertence ao relatório do runner."""

import argparse

from _comum import criar_parser, executar, finalizar, validar_raiz


def main() -> int:
    parser = criar_parser(__doc__)
    parser.add_argument("comando", nargs=argparse.REMAINDER, help="Comando após --; deve falhar em zero testes")
    args = parser.parse_args()
    validar_raiz(parser, args)
    comando = args.comando
    if comando and comando[0] == "--":
        comando = comando[1:]
    if not comando:
        parser.error("Informe o runner após --.")
    resultado = executar(comando, args.raiz, args.timeout)
    return finalizar(args, [resultado])


if __name__ == "__main__":
    raise SystemExit(main())
