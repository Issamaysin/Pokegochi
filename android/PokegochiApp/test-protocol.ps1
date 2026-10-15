param()
$ErrorActionPreference = 'Stop'
$appRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$javaHome = 'C:\Program Files\Android\Android Studio\jbr'
if (-not (Test-Path -LiteralPath (Join-Path $javaHome 'bin\javac.exe'))) {
    $javaHome = Join-Path (Split-Path (Split-Path $appRoot -Parent) -Parent) '.tools\jbr21'
}
$javac = Join-Path $javaHome 'bin\javac.exe'
$java = Join-Path $javaHome 'bin\java.exe'
if (-not (Test-Path -LiteralPath $javac)) { throw 'JDK 17+ nao encontrado.' }
$header = Get-Content -Raw (Join-Path (Split-Path (Split-Path $appRoot -Parent) -Parent) 'include\services\MultiplayerProtocol.h')
$javaProtocol = Get-Content -Raw (Join-Path $appRoot 'app\src\main\java\com\pokegochi\app\MultiplayerProtocol.java')
$pairs = @(
    @('VERSION', 'kVersion', '6'),
    @('PACKET_MAGIC', 'kPacketMagic', '0x4750'),
    @('MAX_PAYLOAD', 'kMaximumPayload', '160'),
    @('SERVICE_UUID', 'kServiceUuid', '70564743-4849-4d50-8000-504f4b45474f'),
    @('RX_UUID', 'kRxUuid', '70564743-4849-4d50-8001-504f4b45474f'),
    @('TX_UUID', 'kTxUuid', '70564743-4849-4d50-8002-504f4b45474f')
)
foreach ($pair in $pairs) {
    if ($javaProtocol -notmatch ([regex]::Escape($pair[0]) + '.*' + [regex]::Escape($pair[2]))) {
        throw "Java protocol diverges from the C++ contract: $($pair[0])"
    }
    if ($header -notmatch ([regex]::Escape($pair[1]) + '.*' + [regex]::Escape($pair[2]))) {
        throw "C++ protocol constant missing: $($pair[1])"
    }
}
$temporary = Join-Path $appRoot 'build\protocol-test-classes'
New-Item -ItemType Directory -Path $temporary -Force | Out-Null
& $javac '-d' $temporary (Join-Path $appRoot 'app\src\main\java\com\pokegochi\app\MultiplayerProtocol.java') (Join-Path $appRoot 'app\src\test\java\com\pokegochi\app\ProtocolSelfTest.java')
if ($LASTEXITCODE -ne 0) { throw 'javac falhou.' }
& $java '-cp' $temporary 'com.pokegochi.app.ProtocolSelfTest'
if ($LASTEXITCODE -ne 0) { throw 'Teste de protocolo falhou.' }
