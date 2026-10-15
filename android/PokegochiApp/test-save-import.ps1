$ErrorActionPreference = 'Stop'
$appRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectRoot = Split-Path (Split-Path $appRoot -Parent) -Parent
$javaHome = 'C:\Program Files\Android\Android Studio\jbr'
if (-not (Test-Path -LiteralPath (Join-Path $javaHome 'bin\javac.exe'))) {
    $javaHome = Join-Path $projectRoot '.tools\jbr21'
}
$output = Join-Path $appRoot 'build\save-import-test-classes'
New-Item -ItemType Directory -Force -Path $output | Out-Null
$importer = Join-Path $appRoot 'app\src\main\java\com\pokegochi\app\PgsavSaveImporter.java'
$test = Join-Path $appRoot 'app\src\test\java\com\pokegochi\app\PgsavSaveImporterSelfTest.java'
& (Join-Path $javaHome 'bin\javac.exe') '-d' $output $importer $test
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
$arguments = @('-cp', $output, 'com.pokegochi.app.PgsavSaveImporterSelfTest')
if ($args.Count -gt 0) { $arguments += $args[0] }
& (Join-Path $javaHome 'bin\java.exe') @arguments
exit $LASTEXITCODE
