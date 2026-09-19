"""Testa os drivers C reais com HAL simulado, sem placa nem pacotes Python extras."""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

def main():
    here = Path(__file__).resolve().parent
    root = here.parents[1]
    core = root / "stm32/Core"
    cc = shutil.which("gcc")
    if not cc:
        raise SystemExit("Instale um GCC nativo e adicione-o ao PATH.")
    with tempfile.TemporaryDirectory(prefix="tcc-sensor-tests-") as tmp:
        build = Path(tmp)
        app = (core / "Src/main.c").read_text(encoding="utf-8")
        def section(begin, end):
            return app.split(begin, 1)[1].split(end, 1)[0]
        def function(signature):
            match = re.search(re.escape(signature) + r"\s*\{", app)
            if match is None:
                raise SystemExit(f"Função não encontrada em main.c: {signature}.")
            index = match.end() - 1
            depth = 0

            for end in range(index, len(app)):
                if app[end] == "{":
                    depth += 1
                elif app[end] == "}":
                    depth -= 1

                    if depth == 0:
                        return app[match.start():end + 1]

            raise SystemExit(f"Função sem fechamento em main.c: {signature}.")
        (build / "app_under_test.h").write_text(
            section("/* USER CODE BEGIN PV */", "/* USER CODE END PV */") +
            section("/* USER CODE BEGIN 0 */", "/* USER CODE END 0 */") +
            function("static void MX_ADC1_Init(void)") +
            "static void iteration(void)\n" + section("while (1)", "/* USER CODE END WHILE */") + "}\n",
            encoding="utf-8")
        flags = [cc, "-std=c11", "-Wall", "-Wextra", "-Werror", "-I", str(here / "host"),
                 "-I", str(core / "Inc"), "-I", str(build)]
        sources = ["dht22", "bmp280", "bh1750", "anemometer", "pluviometer", "wind_vane", "sensor_health", "station_sample"]
        failures = total = 0
        for suite, dispatch in [("test_sensors", "test_dispatch"), ("test_integration", "integration_dispatch")]:
            names = re.findall(r"static void (test_\w+)\(void\)", (here / f"{suite}.c").read_text())
            if not names:
                raise SystemExit(f"Nenhum teste foi encontrado em {suite}.c.")
            (build / f"{dispatch}.h").write_text(
                'int main(int argc,char **argv) { assert(argc==2); fake_reset();\n' +
                ''.join(f'if(!strcmp(argv[1],"{n}")) {{ {n}(); puts("PASS {n}"); return 0; }}\n' for n in names) +
                'return 2; }\n')
            exe = build / f"{suite}.exe"
            extra = [str(core / "Src/storage.c"), str(here / "host/fake_storage.c")] if suite == "test_integration" else []
            subprocess.run(flags + [str(here / f"{suite}.c"), str(here / "host/fake_hal.c")] +
                           [str(core / f"Src/{name}.c") for name in sources] + extra +
                           ["-lm", "-o", str(exe)], check=True, timeout=30)
            for name in names:
                try:
                    result = subprocess.run([str(exe), name], capture_output=True, text=True, errors="replace", timeout=3)
                    ok = result.returncode == 0
                    print(result.stdout.strip().splitlines()[-1] if ok else f"FAIL {name}: {result.stderr.strip()}", flush=True)
                except subprocess.TimeoutExpired:
                    ok = False
                    print(f"FAIL {name}: espera sem limite", flush=True)
                failures += not ok
            total += len(names)
        print(f"{total-failures}/{total} cenarios passaram.")
        return int(failures != 0)

if __name__ == "__main__":
    raise SystemExit(main())
