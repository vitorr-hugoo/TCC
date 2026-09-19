# Firmware

- `stm32/`: projeto STM32CubeIDE/CubeMX da estacao.
- `gateway-esp32/`: receptor LoRa e publicacao MQTT.
- `tests/`: testes host e wrappers de validacao.
- `stm32/patches/`: patches historicos que nao participam do build.

Planos, relatorios e evidencias duradouras ficam em `../docs/engineering/firmware/`.
Os resultados de builds novos devem ser gravados em `../work/firmware/`.
Credenciais reais do gateway sao locais e nao sao versionadas; parta de
`gateway-esp32/gateway_credentials.example.h`.
