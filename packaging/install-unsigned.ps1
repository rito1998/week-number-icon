# this script is intended for local debug installations
param(
    [string]$ValidatedBundlePath
)

$ErrorActionPreference = 'Stop'

$root = Split-Path $PSScriptRoot -Parent
$bundlePath = $ValidatedBundlePath

if (-not $bundlePath) {
    Write-Host 'Checking whether Week Number Icon is already installed...' -ForegroundColor Cyan
    $installedPackages = @(Get-AppxPackage -Name 'RiccardoTorreggiani.WeekNumberIcon')
    if ($installedPackages.Count -gt 0) {
        $installedVersions = @($installedPackages | ForEach-Object { $_.Version.ToString() } | Sort-Object -Unique)
        Write-Warning "App is already installed (version $($installedVersions -join ', ')); it will be removed before installation."
    }
    else {
        Write-Host 'App is not currently installed.' -ForegroundColor Cyan
    }

    Write-Host 'Reading the expected version from build.zig.zon...' -ForegroundColor Cyan
    $buildConfig = Get-Content -LiteralPath (Join-Path $root 'build.zig.zon') -Raw
    $versionMatch = [regex]::Match($buildConfig, '(?m)^\s*\.version\s*=\s*"(?<version>\d+\.\d+\.\d+)"')
    if (-not $versionMatch.Success) {
        throw 'Could not read the three-part version from build.zig.zon.'
    }

    $packageVersion = "$($versionMatch.Groups['version'].Value).0"
    $bundlePath = Join-Path $root "zig-out\msix\WeekNumberIcon-$packageVersion-unsigned.msixbundle"
    Write-Host "Looking for the unsigned bundle matching version $packageVersion..." -ForegroundColor Cyan
    if (-not (Test-Path -LiteralPath $bundlePath -PathType Leaf)) {
        throw "Unsigned MSIX bundle for version $packageVersion was not found. Build it first."
    }

    Write-Host "Found matching unsigned bundle: $bundlePath" -ForegroundColor Cyan
}

$isAdministrator = ([Security.Principal.WindowsPrincipal]::new(
    [Security.Principal.WindowsIdentity]::GetCurrent()
)).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)

if (-not $isAdministrator) {
    Write-Host 'Installing the app requires administrator privileges. Approve the Windows prompt to continue.' -ForegroundColor Cyan

    $currentProcess = Get-Process -Id $PID
    $arguments = @(
        '-NoProfile'
        '-ExecutionPolicy'
        'Bypass'
        '-File'
        "`"$PSCommandPath`""
        '-ValidatedBundlePath'
        "`"$bundlePath`""
    )
    $elevatedProcess = Start-Process -FilePath $currentProcess.Path `
        -ArgumentList $arguments `
        -WorkingDirectory $root `
        -Verb RunAs `
        -Wait `
        -PassThru

    if ($elevatedProcess.ExitCode -ne 0) {
        Write-Error "Elevated installation failed with exit code $($elevatedProcess.ExitCode). See the elevated PowerShell window for details." -ErrorAction Continue
    }
    exit $elevatedProcess.ExitCode
}

Write-Host "Replacing the current installation with $bundlePath" -ForegroundColor Cyan
$installationError = $null
try {
    $installedPackages = @(Get-AppxPackage -Name 'RiccardoTorreggiani.WeekNumberIcon')
    foreach ($package in $installedPackages) {
        Write-Host "Removing installed version $($package.Version)..." -ForegroundColor Cyan
        Remove-AppxPackage -Package $package.PackageFullName
    }

    Add-AppxPackage -Path $bundlePath -AllowUnsigned
}
catch {
    $installationError = $_
}

if ($installationError) {
    Write-Error -ErrorRecord $installationError -ErrorAction Continue
    Read-Host 'Press Enter to close this elevated PowerShell window' | Out-Null
    exit 1
}

Write-Host 'Installation completed.' -ForegroundColor Cyan
Read-Host 'Press Enter to close this elevated PowerShell window' | Out-Null