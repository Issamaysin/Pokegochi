param(
    [ValidateSet('Factory', 'Update')]
    [string]$Mode = 'Factory',
    [string]$Port,
    [switch]$Yes
)

$ErrorActionPreference = 'Stop'
$releaseRoot = $PSScriptRoot
$image = if ($Mode -eq 'Factory') {
    Join-Path $releaseRoot 'firmware\pokegochi-v2.factory.bin'
} else {
    Join-Path $releaseRoot 'firmware\pokegochi-v2.update.bin'
}
$address = if ($Mode -eq 'Factory') { '0x0' } else { '0x20000' }

if (-not (Test-Path -LiteralPath $image)) {
    throw "Imagem não encontrada: $image"
}

function Find-Esp32Ports {
    try {
        return @(Get-PnpDevice -PresentOnly -Class Ports -ErrorAction Stop |
            Where-Object { $_.FriendlyName -match 'CH340|CH341|CP210|USB.?SERIAL|ESP32' } |
            ForEach-Object { if ($_.FriendlyName -match '\((COM\d+)\)') { $Matches[1] } } |
            Sort-Object -Unique)
    } catch {
        return @(Get-CimInstance Win32_SerialPort -ErrorAction SilentlyContinue |
            Where-Object { $_.Description -match 'CH340|CH341|CP210|USB.?SERIAL|ESP32' } |
            ForEach-Object { $_.DeviceID } | Sort-Object -Unique)
    }
}

if (-not $Port) {
    $ports = @(Find-Esp32Ports)
    if ($ports.Count -eq 1) { $Port = $ports[0] }
    elseif ($ports.Count -gt 1) {
        Write-Host "Portas encontradas: $($ports -join ', ')" -ForegroundColor Yellow
        $Port = (Read-Host 'Digite a porta da placa').Trim()
    } else {
        $Port = (Read-Host 'Digite a porta da placa, por exemplo COM6').Trim()
    }
}
$Port = $Port.ToUpperInvariant()
if ($Port -notmatch '^COM\d+$') { throw "Porta inválida: $Port" }

$directEsptool = Get-Command esptool -ErrorAction SilentlyContinue
$python = Get-Command py -ErrorAction SilentlyContinue
if (-not $python) { $python = Get-Command python -ErrorAction SilentlyContinue }
if (-not $directEsptool -and -not $python) {
    throw 'Instale Python 3 e execute: py -m pip install --upgrade esptool'
}

function Invoke-Esptool([string[]]$Arguments) {
    if ($directEsptool) { & $directEsptool.Source @Arguments }
    else { & $python.Source -m esptool @Arguments }
    if ($LASTEXITCODE -ne 0) { throw "esptool terminou com código $LASTEXITCODE" }
}

Write-Host "Modo: $Mode | Porta: $Port | Endereço: $address" -ForegroundColor Cyan
if ($Mode -eq 'Factory' -and -not $Yes) {
    Write-Host 'FACTORY instala bootloader e partições. Use somente em placa nova ou com save dispensável.' -ForegroundColor Yellow
    if ((Read-Host 'Digite GRAVAR para continuar') -cne 'GRAVAR') { throw 'Gravação cancelada.' }
}

Invoke-Esptool @('--chip', 'esp32', '--port', $Port, 'read-mac')
Invoke-Esptool @('--chip', 'esp32', '--port', $Port, '--baud', '460800',
    'write-flash', '--flash-mode', 'dio', '--flash-freq', '40m', '--flash-size',
    '4MB', $address, $image)
Write-Host 'Firmware Pokegochi V2 gravado com sucesso.' -ForegroundColor Green
