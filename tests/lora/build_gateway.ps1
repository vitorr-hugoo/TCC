param(
    [string]$ArduinoCli = 'C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe',
    [string]$ExtraLibraries = (Join-Path $env:TEMP 'tcc-lora-arduino\libraries'),
    [string]$OutputDirectory = (Join-Path $env:TEMP 'tcc-lora-arduino')
)
$ErrorActionPreference = 'Stop'
$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$sourceDirectory = Join-Path $repoRoot 'Firmware\Gateway ESP32'
$sketchDirectory = Join-Path $OutputDirectory 'gateway_esp32_lora'
New-Item -ItemType Directory -Force -Path $sketchDirectory | Out-Null
# Arduino exige que o .ino principal tenha o nome da pasta do sketch.
# Preparar uma copia de compilacao preserva os nomes ja usados no repositorio.
Copy-Item -LiteralPath (Join-Path $sourceDirectory 'gateway_esp32_lora_V1.2.ino') -Destination (Join-Path $sketchDirectory 'gateway_esp32_lora.ino')
foreach ($file in @('lora_protocol.h', 'lora_protocol.cpp', 'lora_receive.h')) {
    Copy-Item -LiteralPath (Join-Path $sourceDirectory $file) -Destination $sketchDirectory
}
$compileArgs = @('compile', '--fqbn', 'esp32:esp32:esp32', '--build-path', (Join-Path $OutputDirectory 'build'))
if (Test-Path -LiteralPath $ExtraLibraries -PathType Container) {
    $compileArgs += @('--libraries', $ExtraLibraries)
}
$compileArgs += $sketchDirectory
& $ArduinoCli @compileArgs
if ($LASTEXITCODE -ne 0) { throw "Compilacao do gateway falhou (exit $LASTEXITCODE)." }
Write-Output "Sketch preparado: $sketchDirectory"
Write-Output "Binarios: $(Join-Path $OutputDirectory 'build')"
