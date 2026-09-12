# Testes de diagnóstico dos sensores

Execute em `Firmware/`, com Python 3 e GCC nativo no PATH:

```powershell
python tests/sensors/run.py
python tests/lora/run.py
```

Não requer placa, bibliotecas Python adicionais nem grava firmware. Compila em diretório temporário com `-std=c11 -Wall -Wextra -Werror`. Cada cenário roda em processo separado, com prazo de 3 segundos.

`test_sensors.c` liga os drivers reais DHT22, BMP280, BH1750, biruta, anemômetro e pluviômetro, além de `sensor_health.c` e `station_sample.c`, a uma HAL simulada. O DHT22 recebe uma forma de onda de 40 bits com tempos independentes do decoder; o BMP280 usa o vetor de compensação do datasheet Bosch. Os testes incluem falhas, recuperação, valores legítimos iguais/zero, rollover dos relógios, limites numéricos e preservação do cache.

`test_integration.c` usa também `storage.c` real com FatFs simulado. O runner extrai diretamente de `main.c` as variáveis, o bloco `USER CODE 0` e o corpo do loop de aquisição. Assim, os testes executam o caminho de finalização e as decisões de serial/SD/LoRa efetivamente usados no firmware, sem manter uma segunda implementação. A simulação substitui somente periféricos, FatFs e a transmissão de rádio. Uma alteração dos delimitadores de `main.c` exige revisar essa extração.

Os testes de integração conferem timeout/erro do timer, leitura antiga do DHT, ADC com erro, I2C compartilhado, sensores sem inicialização, SD ausente/com erro, transições de saúde, LUT real da aplicação e chuva além do formato de transmissão.

Esses testes verificam software e tratamento dos retornos HAL. A compilação ARM verifica integração com a HAL real. Temporização elétrica, desconexão de fios, interferência, calibração e comportamento sob luz solar exigem os testes de bancada descritos no relatório de implementação.
