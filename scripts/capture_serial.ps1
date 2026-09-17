param(
  [string]$Port = 'COM6',
  [string]$LogPath = '.codex-debug\\serial.log',
  [switch]$ResetOnOpen
)

$directory = Split-Path -Parent $LogPath
if ($directory) { New-Item -ItemType Directory -Force -Path $directory | Out-Null }
# COM6 can temporarily disappear when the ESP32 resets or when Windows
# re-enumerates the USB bridge.  Keep the observer alive and reopen it instead
# of silently stopping at the first transient serial exception.
while ($true) {
  $serialPort = $null
  try {
    $serialPort = [System.IO.Ports.SerialPort]::new($Port, 115200, [System.IO.Ports.Parity]::None, 8, [System.IO.Ports.StopBits]::One)
    $serialPort.ReadTimeout = 250
    $serialPort.DtrEnable = $false
    $serialPort.RtsEnable = $false
    $serialPort.Open()
    if ($ResetOnOpen) {
      # Normal reset only: keep GPIO0 released and pulse EN through RTS.  This
      # is useful when attaching the logger immediately after a firmware flash.
      $serialPort.RtsEnable = $true
      Start-Sleep -Milliseconds 80
      $serialPort.RtsEnable = $false
    }
    Add-Content -LiteralPath $LogPath -Value "$(Get-Date -Format o) [SERIAL] connected $Port"
    while ($true) {
      try {
        $line = $serialPort.ReadLine()
        if ($line) { Add-Content -LiteralPath $LogPath -Value "$(Get-Date -Format o) $line" }
      } catch [System.TimeoutException] {}
    }
  } catch {
    Add-Content -LiteralPath $LogPath -Value "$(Get-Date -Format o) [SERIAL] reconnect: $($_.Exception.Message)"
    Start-Sleep -Milliseconds 750
  } finally {
    if ($serialPort) { try { $serialPort.Close() } catch {} ; $serialPort.Dispose() }
  }
}
