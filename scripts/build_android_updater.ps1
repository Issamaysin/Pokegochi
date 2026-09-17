$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$androidRoot = Join-Path $projectRoot 'android\PokegochiUpdater'
$javaHome = 'C:\Program Files\Android\Android Studio\jbr'
$sdk = Join-Path $env:LOCALAPPDATA 'Android\Sdk'
if (-not (Test-Path -LiteralPath (Join-Path $javaHome 'bin\java.exe'))) {
    throw 'Android Studio JBR 17 nao encontrado.'
}
if (-not (Test-Path -LiteralPath $sdk)) { throw 'Android SDK nao encontrado.' }
$env:JAVA_HOME = $javaHome
$env:ANDROID_HOME = $sdk
$env:ANDROID_SDK_ROOT = $sdk
Push-Location $androidRoot
try {
    & .\gradlew.bat :app:assembleDebug
    if ($LASTEXITCODE -ne 0) { throw "Gradle terminou com codigo $LASTEXITCODE." }
} finally { Pop-Location }
$source = Join-Path $androidRoot 'app\build\outputs\apk\debug\app-debug.apk'
$destination = Join-Path $projectRoot 'dist\PokegochiUpdater.apk'
Copy-Item -LiteralPath $source -Destination $destination -Force
Write-Host "Aplicativo Android pronto: $destination" -ForegroundColor Green
