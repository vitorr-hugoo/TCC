"""Compila as variantes STM32 geradas pelo STM32CubeIDE, sem reutilizar binários antigos."""

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

from _comum import CODIGO_NAO_CONFIGURADO, CODIGO_NAO_DISPONIVEL, decodificar_saida, encerrar_arvore, resolver


VARIANTES = ("Debug", "Release")
NOME_VARIAVEL_CUBEIDE = "STM32CUBEIDE_DIR"


def localizar_makefile(diretorio: Path) -> Path | None:
    for nome in ("Makefile", "makefile"):
        caminho = diretorio / nome
        if caminho.is_file():
            return caminho
    return None


def localizar_cubeide(explicito: str | None) -> Path | None:
    """Localiza a instalação do STM32CubeIDE sem alterar o PATH global."""
    candidatos: list[Path] = []
    if explicito:
        candidatos.append(Path(explicito))
    elif os.environ.get(NOME_VARIAVEL_CUBEIDE):
        candidatos.append(Path(os.environ[NOME_VARIAVEL_CUBEIDE]))
    else:
        candidatos.extend(sorted(Path(r"C:\ST").glob("STM32CubeIDE_*/STM32CubeIDE"), reverse=True))

    for candidato in candidatos:
        if (candidato / "plugins").is_dir():
            return candidato.resolve()
    return None


def localizar_ferramentas_cubeide(cubeide: Path) -> tuple[str, dict[str, str]] | None:
    """Retorna make e o ambiente com o ARM GCC da mesma instalação do CubeIDE."""
    plugins = cubeide / "plugins"
    make_bins = sorted(plugins.glob("com.st.stm32cube.ide.mcu.externaltools.make.win32_*/tools/bin"), reverse=True)
    gcc_bins = sorted(
        plugins.glob("com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.*.win32_*/tools/bin"),
        reverse=True,
    )
    for make_bin in make_bins:
        make = make_bin / "make.exe"
        if not make.is_file():
            continue
        for gcc_bin in gcc_bins:
            if (gcc_bin / "arm-none-eabi-gcc.exe").is_file():
                ambiente = os.environ.copy()
                ambiente["PATH"] = os.pathsep.join((str(make_bin), str(gcc_bin), ambiente.get("PATH", "")))
                return str(make), ambiente
    return None


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--raiz", type=Path, default=Path.cwd())
    parser.add_argument("--make", default="make", help="Executável make fornecido pelo STM32CubeIDE")
    parser.add_argument(
        "--stm32cubeide",
        help=f"Diretório STM32CubeIDE; também aceita a variável {NOME_VARIAVEL_CUBEIDE}.",
    )
    parser.add_argument("--jobs", type=int, default=4)
    parser.add_argument("--timeout", type=float, default=60.0)
    parser.add_argument("--diretorio-relatorios", type=Path, default=Path("../work/firmware/reports/build"))
    args = parser.parse_args()
    raiz = args.raiz.resolve()
    if not raiz.is_dir():
        parser.error(f"Raiz inexistente: {raiz}")
    if args.jobs <= 0:
        parser.error("--jobs deve ser positivo.")
    if args.timeout <= 0:
        parser.error("--timeout deve ser positivo.")

    make = shutil.which(args.make)
    ambiente = None
    if make is None:
        cubeide = localizar_cubeide(args.stm32cubeide)
        ferramentas = localizar_ferramentas_cubeide(cubeide) if cubeide else None
        if ferramentas is None:
            print(
                f"Ferramenta de build não disponível: {args.make}. "
                f"Configure {NOME_VARIAVEL_CUBEIDE} ou --stm32cubeide para a instalação do STM32CubeIDE.",
                file=sys.stderr,
            )
            return CODIGO_NAO_DISPONIVEL
        make, ambiente = ferramentas
        print(f"STM32CubeIDE localizado em: {cubeide}")

    diretorios = {variante: raiz / "stm32" / variante for variante in VARIANTES}
    ausentes = [variante for variante, diretorio in diretorios.items() if localizar_makefile(diretorio) is None]
    if ausentes:
        print(
            "Build STM32 não configurado para: " + ", ".join(ausentes) +
            ". Importe stm32 no STM32CubeIDE e gere Debug e Release antes de executar este gate.",
            file=sys.stderr,
        )
        return CODIGO_NAO_CONFIGURADO

    diretorio_relatorios = resolver(raiz, args.diretorio_relatorios)
    diretorio_relatorios.mkdir(parents=True, exist_ok=True)
    for variante, diretorio in diretorios.items():
        processo = subprocess.Popen(
            [make, f"-j{args.jobs}", "all"],
            cwd=diretorio,
            env=ambiente,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
        )
        try:
            bruto, _ = processo.communicate(timeout=args.timeout)
        except subprocess.TimeoutExpired:
            encerrar_arvore(processo)
            bruto, _ = processo.communicate()
            saida = decodificar_saida(bruto) + "\nPrazo excedido.\n"
            codigo = 124
        else:
            saida = decodificar_saida(bruto)
            codigo = processo.returncode
        (diretorio_relatorios / f"{variante}.log").write_text(saida, encoding="utf-8")
        print(f"{variante}: exit {codigo}")
        if saida:
            print(saida, end="" if saida.endswith("\n") else "\n")
        if codigo != 0:
            return codigo
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
