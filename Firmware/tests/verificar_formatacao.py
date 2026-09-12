"""Verifica arquivos explicitamente selecionados, sem modificá-los."""

from pathlib import Path
import shutil

from _comum import CODIGO_NAO_DISPONIVEL, criar_parser, executar, finalizar, resolver, validar_raiz
from _fontes_proprias import ARQUIVOS_FORMATACAO


CAMINHO_CLANG_FORMAT_ARDUINO = Path(
    r"C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\clang-format.exe"
)


def localizarFerramenta(especificacao: str) -> str | None:
    caminho = Path(especificacao)

    if caminho.is_file():
        return str(caminho)

    encontrada = shutil.which(especificacao)

    if encontrada is not None:
        return encontrada

    if (especificacao == "clang-format") and CAMINHO_CLANG_FORMAT_ARDUINO.is_file():
        return str(CAMINHO_CLANG_FORMAT_ARDUINO)

    return None


def main() -> int:
    parser = criar_parser(__doc__)
    parser.add_argument("--ferramenta", default="clang-format")
    parser.add_argument("--proprios", action="store_true", help="Verifica a seleção explícita de fontes próprias")
    parser.add_argument("arquivos", nargs="*", type=Path, help="Fontes/headers próprios; sem varredura de vendor")
    args = parser.parse_args()
    validar_raiz(parser, args)
    if args.proprios and args.arquivos:
        parser.error("Use --proprios ou informe arquivos, não ambos.")
    if not args.proprios and not args.arquivos:
        parser.error("Informe arquivos ou use --proprios.")
    ferramenta = localizarFerramenta(args.ferramenta)
    if ferramenta is None:
        print(f"Ferramenta de formatação não disponível: {args.ferramenta}")
        return CODIGO_NAO_DISPONIVEL
    entradas = ARQUIVOS_FORMATACAO if args.proprios else args.arquivos
    resultados = []
    for entrada in entradas:
        arquivo = resolver(args.raiz, entrada)
        if not arquivo.is_file() or arquivo.suffix.lower() not in {".c", ".h", ".cpp", ".hpp", ".cc", ".cxx", ".ino"}:
            parser.error(f"Fonte/header inválido: {entrada}")
        if not any((pasta / ".clang-format").is_file() or (pasta / "_clang-format").is_file()
                   for pasta in arquivo.parents):
            parser.error(f"Configuração clang-format ausente para {entrada}")
        resultados.append(executar([
            ferramenta, "--style=file", "--dry-run", "--Werror", str(arquivo),
        ], args.raiz, args.timeout))
    return finalizar(args, resultados)


if __name__ == "__main__":
    raise SystemExit(main())
