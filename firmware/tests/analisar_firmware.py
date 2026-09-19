"""Executa analisadores configurados sobre fontes próprias presentes no banco de compilação."""

import argparse
import json
import shutil
import subprocess
import sys
from pathlib import Path

from _comum import CODIGO_NAO_CONFIGURADO, CODIGO_NAO_DISPONIVEL, resolver
from _fontes_proprias import FONTES_ANALISE


CANDIDATOS_BANCO = (
    Path("stm32/Debug/compile_commands.json"),
    Path("stm32/Release/compile_commands.json"),
    Path("stm32/compile_commands.json"),
)

CAMINHO_CLANG_TIDY_VISUAL_STUDIO = Path(
    r"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\Llvm\bin\clang-tidy.exe"
)


def localizar_analisador(nome: str) -> str | None:
    """Localiza analisadores disponíveis sem exigir alteração do PATH global."""
    encontrado = shutil.which(nome)
    if encontrado:
        return encontrado
    if nome == "clang-tidy" and CAMINHO_CLANG_TIDY_VISUAL_STUDIO.is_file():
        return str(CAMINHO_CLANG_TIDY_VISUAL_STUDIO)
    return None


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

    disponiveis = {
        nome: ferramenta
        for nome in ("clang-tidy", "cppcheck")
        if (ferramenta := localizar_analisador(nome)) is not None
    }
    if not disponiveis:
        print("clang-tidy e cppcheck não estão disponíveis.", file=sys.stderr)
        return CODIGO_NAO_DISPONIVEL
    banco = next((resolver(raiz, candidato) for candidato in CANDIDATOS_BANCO
                  if resolver(raiz, candidato).is_file()), None)
    if banco is None:
        print("compile_commands.json atual não foi encontrado após o build.", file=sys.stderr)
        return CODIGO_NAO_CONFIGURADO

    try:
        entradas = json.loads(banco.read_text(encoding="utf-8-sig"))
        fontes_banco = set()
        for entrada in entradas:
            if not isinstance(entrada, dict):
                continue
            diretorio = entrada.get("directory")
            arquivo = entrada.get("file")
            if not isinstance(diretorio, str) or not isinstance(arquivo, str):
                continue
            caminho_diretorio = Path(diretorio)
            if not caminho_diretorio.is_absolute():
                caminho_diretorio = banco.parent / caminho_diretorio
            fontes_banco.add((caminho_diretorio / arquivo).resolve())
    except (OSError, ValueError, TypeError, json.JSONDecodeError) as erro:
        print(f"compile_commands.json inválido: {erro}", file=sys.stderr)
        return CODIGO_NAO_CONFIGURADO

    wrapper = raiz / "tests" / "executar_analise_estatica.py"
    fontes = [
        str(arquivo)
        for arquivo in FONTES_ANALISE
        if (raiz / arquivo).resolve() in fontes_banco
    ]
    if "clang-tidy" in disponiveis and not fontes:
        print("O banco não contém fontes próprias selecionadas para clang-tidy.", file=sys.stderr)
        return CODIGO_NAO_CONFIGURADO
    for analisador, ferramenta in disponiveis.items():
        comando = [
            sys.executable,
            str(wrapper),
            "--raiz", str(raiz),
            "--timeout", str(args.timeout),
            "--analisador", analisador,
            "--ferramenta", ferramenta,
            "--banco", str(banco),
        ]
        if analisador == "clang-tidy":
            comando.extend(fontes)
        resultado = subprocess.run(comando, cwd=raiz, check=False)
        if resultado.returncode != 0:
            return resultado.returncode
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
