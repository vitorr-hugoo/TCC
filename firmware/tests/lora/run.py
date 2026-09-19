"""Compila o driver real com HAL fake e executa cada cenario em processo isolado."""
import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile

CASES = (
    "mode_delayed", "mode_stuck", "mode_spi_error", "mode_wrap",
    "invalid_arguments", "init_ok", "init_write_error", "init_read_error",
    "init_mismatch", "init_final_read_error", "tx_ok", "radio_ff", "wrong_version", "read_error",
    "write_error", "fifo_error", "stale_done", "disconnect_during_tx",
    "disconnect_after_done", "tx_timeout", "busy_timeout",
    "header_error", "lost_config", "wrong_length", "bad_final_mode",
    "timeout_wrap", "final_irq_error", "transient_retry", "uninitialized", "reset_radio",
)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cc", default="gcc", help="compilador C nativo; nao usar arm-none-eabi-gcc")
    parser.add_argument("--case", choices=CASES)
    args = parser.parse_args()
    cc = shutil.which(args.cc)
    if not cc:
        parser.error(f"Compilador nao encontrado: {args.cc}")
    here = Path(__file__).resolve().parent
    root = here.parents[1]
    failures = 0
    cases = (args.case,) if args.case else CASES
    if not cases:
        parser.error("Nenhum cenario de teste foi configurado.")
    with tempfile.TemporaryDirectory(prefix="tcc-lora-tests-") as build:
        exe = Path(build) / "test_driver.exe"
        command = [cc, "-std=c11", "-Wall", "-Wextra", "-Werror",
                   "-I", str(here / "host"),
                   "-I", str(root / "stm32/Core/Inc"),
                   str(here / "test_driver.c"),
                   str(root / "stm32/Core/Src/lora_sx1276.c"),
                   "-o", str(exe)]
        subprocess.run(command, check=True, timeout=30)
        for case in cases:
            try:
                result = subprocess.run([str(exe), case], capture_output=True,
                                        text=True, errors="replace", timeout=3)
                ok = result.returncode == 0
                detail = result.stderr.strip() or f"exit={result.returncode}"
            except subprocess.TimeoutExpired:
                ok = False
                detail = "processo excedeu 3s: espera sem limite no driver"
            if not ok:
                failures += 1
            print(f"{'PASS' if ok else 'FAIL'} {case}" + (f": {detail}" if not ok else ""), flush=True)
        if not args.case:
            payload_exe = Path(build) / "test_payload.exe"
            subprocess.run([cc, "-std=c11", "-Wall", "-Wextra", "-Werror",
                            "-I", str(root / "stm32/Core/Inc"),
                            str(here / "test_payload.c"), "-o", str(payload_exe)], check=True, timeout=30)
            result = subprocess.run([str(payload_exe)], timeout=3)
            failures += result.returncode != 0
            cxx = shutil.which("g++")
            if not cxx:
                parser.error("Compilador C++ nativo g++ nao encontrado")
            gateway_exe = Path(build) / "test_gateway.exe"
            subprocess.run([cxx, "-std=c++11", "-Wall", "-Wextra", "-Werror",
                            "-I", str(root / "gateway-esp32"),
                            str(here / "test_gateway.cpp"), str(root / "gateway-esp32/lora_protocol.cpp"),
                            "-o", str(gateway_exe)], check=True, timeout=30)
            for case in range(1, 24):
                result = subprocess.run([str(gateway_exe), str(case)], timeout=3)
                failures += result.returncode != 0
    total = len(cases) + (24 if not args.case else 0)
    print(f"\n{total - failures}/{total} passaram; {failures} falharam.")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
