param(
    [string]$ArduinoCli = 'C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe',
    [string]$ExtraLibraries = (Join-Path $env:TEMP 'tcc-lora-arduino\libraries'),
    [string]$OutputDirectory = (Join-Path $env:TEMP 'tcc-lora-arduino'),
    [ValidateRange(1, 64)]
    [int]$Jobs = 1,
    [ValidateSet('none', 'default', 'more', 'all')]
    [string]$Warnings = 'default',
    [string]$StatusFile = ''
)
$ErrorActionPreference = 'Stop'
$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$sourceDirectory = Join-Path $repoRoot 'Gateway ESP32'
$sketchDirectory = Join-Path $OutputDirectory 'gateway_esp32_lora'
New-Item -ItemType Directory -Force -Path $sketchDirectory | Out-Null
# Arduino exige que o .ino principal tenha o nome da pasta do sketch.
# Preparar uma copia de compilacao preserva os nomes ja usados no repositorio.
Copy-Item -LiteralPath (Join-Path $sourceDirectory 'gateway_esp32_lora_V1.2.ino') -Destination (Join-Path $sketchDirectory 'gateway_esp32_lora.ino')
foreach ($file in @('lora_protocol.h', 'lora_protocol.cpp', 'lora_receive.h')) {
    Copy-Item -LiteralPath (Join-Path $sourceDirectory $file) -Destination $sketchDirectory
}
$credentialsFile = Join-Path $sourceDirectory 'gateway_credentials.h'
if (-not (Test-Path -LiteralPath $credentialsFile -PathType Leaf)) {
    throw "Credenciais ausentes: copie gateway_credentials.example.h para gateway_credentials.h e preencha os valores locais."
}
Copy-Item -LiteralPath $credentialsFile -Destination $sketchDirectory
$compileArgs = @(
    'compile',
    '--fqbn', 'esp32:esp32:esp32',
    '--jobs', $Jobs,
    '--warnings', $Warnings,
    '--build-path', (Join-Path $OutputDirectory 'build')
)
if (Test-Path -LiteralPath $ExtraLibraries -PathType Container) {
    $compileArgs += @('--libraries', $ExtraLibraries)
}
$compileArgs += $sketchDirectory
& $ArduinoCli @compileArgs
if ($LASTEXITCODE -ne 0) { throw "Compilacao do gateway falhou (exit $LASTEXITCODE)." }
Write-Output "Sketch preparado: $sketchDirectory"
Write-Output "Binarios: $(Join-Path $OutputDirectory 'build')"
if (-not [string]::IsNullOrWhiteSpace($StatusFile)) {
    $statusDirectory = Split-Path -Parent $StatusFile
    if ($statusDirectory) {
        New-Item -ItemType Directory -Force -Path $statusDirectory | Out-Null
    }
    [System.IO.File]::WriteAllText($StatusFile, "PASS`n", [System.Text.UTF8Encoding]::new($false))
}
