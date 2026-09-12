"""Execução compartilhada dos templates; somente biblioteca padrão, Python 3.10+."""

import argparse
import json
import locale
import os
import subprocess
import sys
import time
from pathlib import Path


CODIGO_NAO_CONFIGURADO = 125
CODIGO_NAO_DISPONIVEL = 127


def encerrar_arvore(processo: subprocess.Popen[bytes]) -> None:
    """Encerra o processo e seus filhos quando o prazo de um gate expira."""
    if os.name == "nt":
        subprocess.run(
            ["taskkill", "/PID", str(processo.pid), "/T", "/F"],
            check=False,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
    else:
        processo.kill()


def decodificar_saida(saida: bytes | str | None) -> str:
    """Decodifica UTF-8 quando disponível e preserva saída de ferramentas locais."""
    if saida is None:
        return ""
    if isinstance(saida, str):
        texto = saida
    else:
        try:
            texto = saida.decode("utf-8")
        except UnicodeDecodeError:
            texto = saida.decode(locale.getpreferredencoding(False), errors="replace")
    return texto.replace("\r\n", "\n").replace("\r", "\n")


def prazo_positivo(texto: str) -> float:
    """Rejeita prazos nulos, infinitos e NaN."""
    import math

    valor = float(texto)
    if not math.isfinite(valor) or valor <= 0:
        raise argparse.ArgumentTypeError("O timeout deve ser positivo e finito.")
    return valor


def criar_parser(descricao: str) -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=descricao)
    parser.add_argument("--raiz", type=Path, default=Path.cwd())
    parser.add_argument("--timeout", type=prazo_positivo, default=300.0)
    parser.add_argument("--relatorio", type=Path, help="JSON relativo à raiz ou caminho explícito")
    return parser


def resolver(raiz: Path, caminho: Path) -> Path:
    return caminho.resolve() if caminho.is_absolute() else (raiz / caminho).resolve()


def executar(comando: list[str], raiz: Path, timeout: float) -> dict:
    inicio = time.monotonic()
    resultado = {"comando": comando, "diretorio": str(raiz)}
    try:
        processo = subprocess.Popen(
            comando, cwd=raiz, shell=False,
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        )
        try:
            bruto, _ = processo.communicate(timeout=timeout)
        except subprocess.TimeoutExpired:
            encerrar_arvore(processo)
            bruto, _ = processo.communicate()
            resultado.update(
                status="TIMEOUT",
                returncode=None,
                saida=decodificar_saida(bruto) + "\nPrazo excedido.",
            )
        else:
            saida = decodificar_saida(bruto)
            status = "PASS"
            if processo.returncode == CODIGO_NAO_CONFIGURADO:
                status = "NÃO CONFIGURADO"
            elif processo.returncode == CODIGO_NAO_DISPONIVEL:
                status = "NÃO DISPONÍVEL"
            elif processo.returncode != 0:
                status = "FAIL"
            resultado.update(status=status, returncode=processo.returncode, saida=saida)
    except FileNotFoundError as erro:
        resultado.update(status="NÃO DISPONÍVEL", returncode=None, saida=str(erro))
    except OSError as erro:
        resultado.update(status="ERRO", returncode=None, saida=str(erro))
    resultado["duracao_segundos"] = round(time.monotonic() - inicio, 3)
    print(f"{resultado['status']}: {comando!r}")
    if resultado["saida"]:
        print(resultado["saida"], end="" if resultado["saida"].endswith("\n") else "\n")
    return resultado


def gravar_relatorio(caminho: Path, conteudo: dict) -> None:
    """Substitui relatório somente após escrita completa."""
    import os
    import tempfile

    caminho.parent.mkdir(parents=True, exist_ok=True)
    temporario = None
    try:
        with tempfile.NamedTemporaryFile(
            mode="w", encoding="utf-8", dir=caminho.parent, delete=False, suffix=".tmp"
        ) as arquivo:
            temporario = Path(arquivo.name)
            json.dump(conteudo, arquivo, ensure_ascii=False, indent=2)
            arquivo.write("\n")
        os.replace(temporario, caminho)
    finally:
        if temporario is not None:
            temporario.unlink(missing_ok=True)


def finalizar(args: argparse.Namespace, resultados: list[dict]) -> int:
    if not resultados:
        resultados = [{"status": "NÃO CONFIGURADO", "saida": "Nenhuma entrada selecionada."}]
    codigo = 0 if all(item["status"] == "PASS" for item in resultados) else 1
    if args.relatorio:
        try:
            gravar_relatorio(resolver(args.raiz, args.relatorio), {
                "resultado": "PASS" if codigo == 0 else "PENDÊNCIA OU FALHA",
                "execucoes": resultados,
            })
        except OSError as erro:
            print(f"Não foi possível gravar relatório: {erro}", file=sys.stderr)
            return 2
    return codigo


def validar_raiz(parser: argparse.ArgumentParser, args: argparse.Namespace) -> None:
    args.raiz = args.raiz.resolve()
    if not args.raiz.is_dir():
        parser.error(f"Raiz inexistente: {args.raiz}")
