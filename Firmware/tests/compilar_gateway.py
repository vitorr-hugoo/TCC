"""Compila o gateway ESP32 por meio do script Arduino existente, sem gravar hardware."""

import argparse
import shutil
import subprocess
import sys
import time
from pathlib import Path

from _comum import CODIGO_NAO_DISPONIVEL, decodificar_saida, encerrar_arvore, resolver


CAMINHO_ARDUINO_CLI_PADRAO = Path(
    r"C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"
)


def localizar_executavel(especificacao: str) -> str | None:
    caminho = Path(especificacao)
    if caminho.is_file():
        return str(caminho)
    return shutil.which(especificacao)


def limpar_saida_anterior(raiz: Path, diretorio_saida: Path) -> None:
    """Remove apenas a saída Arduino anterior dentro de relatorios antes de um build novo."""
    relatorios = (raiz / "relatorios").resolve()
    destino = diretorio_saida.resolve()
    if not destino.is_relative_to(relatorios):
        raise ValueError(f"Diretório de saída fora de relatorios: {destino}")
    if destino.exists():
        shutil.rmtree(destino)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--raiz", type=Path, default=Path.cwd())
    parser.add_argument("--arduino-cli", default=str(CAMINHO_ARDUINO_CLI_PADRAO))
    parser.add_argument("--powershell", default="powershell")
    parser.add_argument("--jobs", type=int, default=1, help="Compila serialmente por padrão para evitar corrida no archive core.a.")
    parser.add_argument("--timeout", type=float, default=300.0)
    parser.add_argument("--diretorio-relatorios", type=Path, default=Path("relatorios/build"))
    args = parser.parse_args()
    raiz = args.raiz.resolve()
    if not raiz.is_dir():
        parser.error(f"Raiz inexistente: {raiz}")
    if args.timeout <= 0:
        parser.error("--timeout deve ser positivo.")
    if args.jobs <= 0:
        parser.error("--jobs deve ser positivo.")
    arduino_cli = localizar_executavel(args.arduino_cli)
    if arduino_cli is None:
        print(f"Ferramenta de build não disponível: {args.arduino_cli}", file=sys.stderr)
        return CODIGO_NAO_DISPONIVEL
    powershell = localizar_executavel(args.powershell)
    if powershell is None:
        print(f"PowerShell não disponível: {args.powershell}", file=sys.stderr)
        return CODIGO_NAO_DISPONIVEL

    script = raiz / "tests" / "lora" / "build_gateway.ps1"
    if not script.is_file():
        print(f"Script de build inexistente: {script}", file=sys.stderr)
        return 1
    diretorio_relatorios = resolver(raiz, args.diretorio_relatorios)
    diretorio_relatorios.mkdir(parents=True, exist_ok=True)
    saida = diretorio_relatorios / "gateway.log"
    diretorio_saida = raiz / "relatorios" / "arduino"
    marcador_sucesso = diretorio_relatorios / "gateway.status"
    try:
        limpar_saida_anterior(raiz, diretorio_saida)
    except (OSError, ValueError) as erro:
        print(f"Não foi possível preparar saída Arduino: {erro}", file=sys.stderr)
        return 1
    marcador_sucesso.unlink(missing_ok=True)
    processo = subprocess.Popen(
        [
            powershell,
            "-NoProfile",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            str(script),
            "-ArduinoCli",
            arduino_cli,
            "-OutputDirectory",
            str(diretorio_saida),
            "-Jobs",
            str(args.jobs),
            "-StatusFile",
            str(marcador_sucesso),
        ],
        cwd=raiz,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    limite = time.monotonic() + args.timeout
    sucesso_confirmado = False
    while processo.poll() is None and time.monotonic() < limite:
        if marcador_sucesso.is_file() and marcador_sucesso.read_text(encoding="utf-8").strip() == "PASS":
            sucesso_confirmado = True
            break
        time.sleep(0.1)
    if sucesso_confirmado:
        try:
            bruto, _ = processo.communicate(timeout=5.0)
        except subprocess.TimeoutExpired:
            encerrar_arvore(processo)
            bruto, _ = processo.communicate()
        texto = decodificar_saida(bruto)
        codigo = 0
    elif processo.poll() is None:
        encerrar_arvore(processo)
        bruto, _ = processo.communicate()
        texto = decodificar_saida(bruto) + "\nPrazo excedido.\n"
        codigo = 124
    else:
        bruto, _ = processo.communicate()
        texto = decodificar_saida(bruto)
        codigo = processo.returncode
    marcador_sucesso.unlink(missing_ok=True)
    saida.write_text(texto, encoding="utf-8")
    if codigo != 0:
        try:
            shutil.rmtree(diretorio_saida)
        except OSError as erro:
            print(f"Não foi possível remover saída parcial: {erro}", file=sys.stderr)
    if texto:
        print(texto, end="" if texto.endswith("\n") else "\n")
    return codigo


if __name__ == "__main__":
    raise SystemExit(main())
