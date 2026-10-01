$ErrorActionPreference = 'Stop'

$isElevated = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
    [Security.Principal.WindowsBuiltInRole]::Administrator)

if (-not $isElevated) {
    Write-Host 'Not elevated - requesting administrator privileges...'
    $exe = if ($PSVersionTable.PSEdition -eq 'Core') { 'pwsh.exe' } else { 'powershell.exe' }
    Start-Process -FilePath $exe -Verb RunAs -ArgumentList @(
        '-NoProfile'
        '-ExecutionPolicy', 'Bypass'
        '-NoExit'
        '-File', "`"$PSCommandPath`""
    )
    return
}

# Resolved from the script location so the elevated session's working directory doesn't matter.
$bundle = Get-ChildItem (Join-Path $PSScriptRoot '..\zig-out\msix\*.msixbundle') |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1
if (-not $bundle) { throw 'No .msixbundle found. Run build.ps1 then packaging/package.ps1 first.' }

Add-AppxPackage -Path $bundle.FullName -AllowUnsigned
