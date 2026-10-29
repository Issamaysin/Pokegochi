param([switch]$EnableNativeCore, [switch]$Release)
$ErrorActionPreference = 'Stop'
# Kept for compatibility with older local commands. The firmware runtime is
# mandatory now; there is no simplified Java/fallback game in this module.
$appRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$wrapper = Join-Path (Join-Path $appRoot '..') 'PokegochiUpdater\gradlew.bat'
$javaHome = 'C:\Program Files\Android\Android Studio\jbr'
if (-not (Test-Path -LiteralPath (Join-Path $javaHome 'bin\java.exe'))) {
    $javaHome = Join-Path (Split-Path (Split-Path $appRoot -Parent) -Parent) '.tools\jbr21'
}
if (-not (Test-Path -LiteralPath (Join-Path $javaHome 'bin\java.exe'))) {
    throw 'JDK 17+ nao encontrado.'
}
$env:JAVA_HOME = $javaHome
$env:PATH = (Join-Path $javaHome 'bin') + ';' + $env:PATH
$sdk = if ($env:POKEGOCHI_ANDROID_SDK) {
    $env:POKEGOCHI_ANDROID_SDK
} else {
    Join-Path $env:LOCALAPPDATA 'Android\Sdk'
}
if (-not (Test-Path -LiteralPath $sdk)) { throw 'Android SDK nao encontrado.' }
$env:ANDROID_HOME = $sdk
$env:ANDROID_SDK_ROOT = $sdk
$env:POKEGOCHI_ENABLE_NDK = '1'
$ndk = Join-Path (Split-Path (Split-Path $appRoot -Parent) -Parent) '.tools\android-ndk-r27d'
if (Test-Path -LiteralPath $ndk) {
    $env:ANDROID_NDK_HOME = $ndk
    $env:ANDROID_NDK_ROOT = $ndk
}
Push-Location $appRoot
try {
    $task = if ($Release) { ':app:verifyReleaseApkAssetPack' } else { ':app:verifyDebugApkAssetPack' }
    & $wrapper '-p' $appRoot $task
    if ($LASTEXITCODE -ne 0) { throw "Gradle terminou com codigo $LASTEXITCODE." }
} finally { Pop-Location }
Write-Host ("PokegochiApp compilado ({0})." -f $(if ($Release) { 'release' } else { 'debug' })) -ForegroundColor Green
