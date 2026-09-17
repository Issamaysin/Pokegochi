param(
  [string]$Port = 'COM7',
  [int]$Cycles = 3,
  [int]$SecondsPerCycle = 34,
  [string]$LogPath = '.generated\sd-reliability.log'
)

$ErrorActionPreference = 'Stop'
$resolvedLogPath = [System.IO.Path]::GetFullPath($LogPath)
New-Item -ItemType Directory -Force -Path ([System.IO.Path]::GetDirectoryName($resolvedLogPath)) | Out-Null
Remove-Item -LiteralPath $resolvedLogPath -ErrorAction SilentlyContinue

for ($cycle = 1; $cycle -le $Cycles; $cycle++) {
  $serial = [System.IO.Ports.SerialPort]::new($Port, 115200, 'None', 8, 'One')
  $serial.ReadTimeout = 150
  try {
    $serial.Open()
    $serial.DiscardInBuffer()
    # Normal ESP32 auto-reset: GPIO0 released; RTS pulses EN.
    $serial.DtrEnable = $false
    $serial.RtsEnable = $true
    Start-Sleep -Milliseconds 120
    $serial.RtsEnable = $false
    $deadline = [DateTime]::UtcNow.AddSeconds($SecondsPerCycle)
    $log = [System.Text.StringBuilder]::new()
    while ([DateTime]::UtcNow -lt $deadline) {
      $chunk = $serial.ReadExisting()
      if ($chunk.Length) { [void]$log.Append($chunk) }
      Start-Sleep -Milliseconds 60
    }
    $text = $log.ToString()
    $mounted = $text -match '\[BOOT\] microSD OK'
    $assets = $text -match '\[BOOT\] sprite asset pack OK'
    $failed = $text -match '\[BOOT\] microSD FAIL'
    Add-Content -LiteralPath $resolvedLogPath -Value ("=== cycle $cycle $(Get-Date -Format o) mounted=$mounted assets=$assets failed=$failed ===")
    Add-Content -LiteralPath $resolvedLogPath -Value $text
  } catch {
    Add-Content -LiteralPath $resolvedLogPath -Value ("=== cycle $cycle exception: $($_.Exception.Message) ===")
  } finally {
    if ($serial.IsOpen) { $serial.Close() }
    $serial.Dispose()
  }
}
