"""Executa clang-tidy ou cppcheck sobre compilation database existente."""

import json
from pathlib import Path

from _comum import criar_parser, executar, finalizar, resolver, validar_raiz


def main() -> int:
    parser = criar_parser(__doc__)
    parser.add_argument("--analisador", choices=["clang-tidy", "cppcheck"], required=True)
    parser.add_argument("--ferramenta", help="Executável alternativo")
    parser.add_argument("--banco", type=Path, required=True, help="Caminho para compile_commands.json")
    parser.add_argument("--extra", action="append", default=[], help="Argumento adicional; use --extra=--opcao")
    parser.add_argument("arquivos", nargs="*", type=Path)
    args = parser.parse_args()
    validar_raiz(parser, args)
    banco = resolver(args.raiz, args.banco)
    if banco.name != "compile_commands.json" or not banco.is_file():
        parser.error("Informe um compile_commands.json existente.")
    try:
        entradas = json.loads(banco.read_text(encoding="utf-8-sig"))
        if not isinstance(entradas, list) or not entradas:
            raise ValueError("Banco vazio ou inválido.")
        fontes = set()
        for entrada in entradas:
            if not isinstance(entrada, dict) or not all(isinstance(entrada.get(campo), str)
                                                       for campo in ("directory", "file")):
                raise ValueError("Entrada sem directory/file válidos.")
            argumentos = entrada.get("arguments")
            comando = entrada.get("command")
            if not ((isinstance(comando, str) and comando.strip()) or
                    (isinstance(argumentos, list) and argumentos and
                     all(isinstance(item, str) for item in argumentos))):
                raise ValueError("Entrada sem command/arguments válidos.")
            diretorio = resolver(banco.parent, Path(entrada["directory"]))
            fontes.add(resolver(diretorio, Path(entrada["file"])))
    except (OSError, ValueError) as erro:
        parser.error(f"Compilation database inválido: {erro}")
    ferramenta = args.ferramenta or args.analisador
    if args.analisador == "clang-tidy":
        if not args.arquivos:
            parser.error("clang-tidy exige seleção explícita de fontes.")
        comandos = []
        for entrada in args.arquivos:
            arquivo = resolver(args.raiz, entrada)
            if not arquivo.is_file():
                parser.error(f"Arquivo inexistente: {entrada}")
            if arquivo not in fontes:
                parser.error(f"Fonte sem comando no banco: {entrada}")
            comandos.append([
                ferramenta, str(arquivo), "-p", str(banco.parent),
                *args.extra, "--warnings-as-errors=*",
            ])
    else:
        if args.arquivos:
            parser.error("cppcheck usa o banco completo; filtre-o no projeto ou use --extra=-iCAMINHO.")
        comandos = [[
            ferramenta, f"--project={banco}", "--enable=warning,style,performance,portability",
            *args.extra, "--error-exitcode=1",
        ]]
    resultados = [executar(comando, args.raiz, args.timeout) for comando in comandos]
    return finalizar(args, resultados)


if __name__ == "__main__":
    raise SystemExit(main())
