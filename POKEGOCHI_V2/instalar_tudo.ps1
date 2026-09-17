param(
    [Parameter(Mandatory = $true)][string]$Drive,
    [string]$Port,
    [ValidateSet('Factory', 'Update')][string]$Mode = 'Factory',
    [switch]$Format,
    [switch]$Yes
)

$ErrorActionPreference = 'Stop'
$sdArgs = @{ Drive = $Drive; Format = $Format; Yes = $Yes }
& (Join-Path $PSScriptRoot 'instalar_sd.ps1') @sdArgs
$fwArgs = @{ Mode = $Mode; Port = $Port; Yes = $Yes }
& (Join-Path $PSScriptRoot 'instalar_firmware.ps1') @fwArgs
