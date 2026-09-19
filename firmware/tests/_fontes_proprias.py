"""Seleção explícita de fontes próprias incluídas nos gates de validação."""

from pathlib import Path


ARQUIVOS_FORMATACAO = (
    Path("stm32/Core/Inc/anemometer.h"),
    Path("stm32/Core/Inc/bh1750.h"),
    Path("stm32/Core/Inc/bmp280.h"),
    Path("stm32/Core/Inc/dht22.h"),
    Path("stm32/Core/Inc/lora_payload.h"),
    Path("stm32/Core/Inc/lora_sx1276.h"),
    Path("stm32/Core/Inc/pluviometer.h"),
    Path("stm32/Core/Inc/sd_card.h"),
    Path("stm32/Core/Inc/sd_spi.h"),
    Path("stm32/Core/Inc/sensor_health.h"),
    Path("stm32/Core/Inc/station_sample.h"),
    Path("stm32/Core/Inc/storage.h"),
    Path("stm32/Core/Inc/wind_vane.h"),
    Path("stm32/Core/Src/anemometer.c"),
    Path("stm32/Core/Src/bh1750.c"),
    Path("stm32/Core/Src/bmp280.c"),
    Path("stm32/Core/Src/dht22.c"),
    Path("stm32/Core/Src/lora_sx1276.c"),
    Path("stm32/Core/Src/pluviometer.c"),
    Path("stm32/Core/Src/sd_card.c"),
    Path("stm32/Core/Src/sd_spi.c"),
    Path("stm32/Core/Src/sensor_health.c"),
    Path("stm32/Core/Src/station_sample.c"),
    Path("stm32/Core/Src/storage.c"),
    Path("stm32/Core/Src/wind_vane.c"),
    Path("gateway-esp32/gateway_esp32_lora_V1.2.ino"),
    Path("gateway-esp32/gateway_credentials.example.h"),
    Path("gateway-esp32/lora_protocol.h"),
    Path("gateway-esp32/lora_protocol.cpp"),
    Path("gateway-esp32/lora_receive.h"),
    Path("tests/lora/fixtures.h"),
    Path("tests/lora/test_driver.c"),
    Path("tests/lora/test_gateway.cpp"),
    Path("tests/lora/test_payload.c"),
    Path("tests/lora/host/stm32f4xx_hal.h"),
    Path("tests/sensors/test_integration.c"),
    Path("tests/sensors/test_sensors.c"),
    Path("tests/sensors/host/fake_hal.c"),
    Path("tests/sensors/host/fake_hal.h"),
    Path("tests/sensors/host/fake_storage.c"),
    Path("tests/sensors/host/fake_storage.h"),
    Path("tests/sensors/host/ff.h"),
    Path("tests/sensors/host/sd_card.h"),
    Path("tests/sensors/host/sd_spi.h"),
    Path("tests/sensors/host/stm32f4xx_hal.h"),
)

FONTES_ANALISE = tuple(
    arquivo
    for arquivo in ARQUIVOS_FORMATACAO
    if arquivo.suffix.lower() in {".c", ".cpp"} and "tests/" not in arquivo.as_posix()
)
