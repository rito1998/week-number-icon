param(
    [switch]$Unsigned,
    [ValidatePattern('^\d+\.\d+\.\d+\.\d+$')][string]$Version,
    [string]$CertificateThumbprint,
    # Values reserved in Partner Center; the Store signs the uploaded bundle itself.
    [string]$StorePublisher,
    [string]$IdentityName,
    [string]$PublisherDisplayName,
    [string]$DisplayName
)

$ErrorActionPreference = 'Stop'
if ($Unsigned -and $StorePublisher) {
    throw 'Do not specify -StorePublisher with -Unsigned.'
}
if (-not $Unsigned) {
    if (-not $StorePublisher) { $StorePublisher = 'CN=CA91C2C7-9980-4F29-AF84-02B407E705F7' }
    if (-not $IdentityName) { $IdentityName = 'RiccardoTorreggiani.WeekNumberIcon' }
    if (-not $PublisherDisplayName) { $PublisherDisplayName = 'Riccardo Torreggiani' }
    if (-not $DisplayName) { $DisplayName = 'Week Number Icon' }
}

$root = Split-Path $PSScriptRoot -Parent
$zonPath = Join-Path $root 'build.zig.zon'
if (-not $Version) {
    $zonContent = Get-Content $zonPath -Raw
    if ($zonContent -notmatch '(?m)^\s*\.version\s*=\s*"(?<semver>\d+\.\d+\.\d+)"\s*,?\s*$') {
        throw "Could not find a three-part SemVer .version in $zonPath."
    }
    $Version = "$($Matches.semver).0"
}
$exeName = 'week-number-icon.exe'

# Zig cpu arch tag -> MSIX ProcessorArchitecture
$targets = [ordered]@{
    'x86_64'  = 'x64'
    'x86'     = 'x86'
    'aarch64' = 'arm64'
}

$sdkBin = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\bin'
$sdk = Get-ChildItem $sdkBin -Directory | Sort-Object Name -Descending |
    Where-Object { Test-Path (Join-Path $_.FullName 'x64\makeappx.exe') } |
    Select-Object -First 1
if (-not $sdk) { throw 'Windows SDK makeappx.exe is required.' }
$makeappx = Join-Path $sdk.FullName 'x64\makeappx.exe'
$signtool = Join-Path $sdk.FullName 'x64\signtool.exe'

if ($StorePublisher -and $CertificateThumbprint) {
    throw 'Use either -StorePublisher or -CertificateThumbprint, not both.'
}

# The OID marks a local package as installable via Add-AppxPackage -AllowUnsigned.
$publisher = 'CN=Riccardo Torreggiani, OID.2.25.311729368913984317654407730594956997722=1'
if ($StorePublisher) {
    $publisher = $StorePublisher
}
elseif ($CertificateThumbprint) {
    $certificate = Get-Item "Cert:\CurrentUser\My\$CertificateThumbprint" -ErrorAction Stop
    if (-not $certificate.HasPrivateKey) { throw 'Signing certificate needs a private key.' }
    $publisher = $certificate.Subject
}

Add-Type -AssemblyName System.Drawing

# makeappx bundle requires a folder containing only the packages to bundle.
$packages = Join-Path $root 'zig-out\msix\packages'
if (Test-Path $packages) { Remove-Item $packages -Recurse -Force }
New-Item -ItemType Directory -Force $packages | Out-Null

foreach ($target in $targets.GetEnumerator()) {
    $architecture = $target.Value
    $exe = Join-Path $root "zig-out\$($target.Key)\$exeName"
    if (-not (Test-Path $exe)) { throw "Build the executables first (build.ps1): $exe" }

    $stage = Join-Path $root "zig-out\msix\stage\$architecture"
    if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
    New-Item -ItemType Directory -Force (Join-Path $stage 'assets') | Out-Null
    Copy-Item $exe $stage -Force

    [xml]$manifest = Get-Content (Join-Path $PSScriptRoot 'AppxManifest.xml')
    $namespace = [Xml.XmlNamespaceManager]::new($manifest.NameTable)
    $namespace.AddNamespace('appx', 'http://schemas.microsoft.com/appx/manifest/foundation/windows10')
    $identity = $manifest.SelectSingleNode('/appx:Package/appx:Identity', $namespace)
    $identity.SetAttribute('ProcessorArchitecture', $architecture)
    $identity.SetAttribute('Publisher', $publisher)
    $identity.SetAttribute('Version', $Version)
    if ($IdentityName) { $identity.SetAttribute('Name', $IdentityName) }
    if ($PublisherDisplayName) {
        $manifest.SelectSingleNode('/appx:Package/appx:Properties/appx:PublisherDisplayName', $namespace).InnerText = $PublisherDisplayName
    }
    if ($DisplayName) {
        $manifest.SelectSingleNode('/appx:Package/appx:Properties/appx:DisplayName', $namespace).InnerText = $DisplayName
    }
    $manifest.Save((Join-Path $stage 'AppxManifest.xml'))

    Copy-Item ./assets/logo-44x44.png (Join-Path $stage "assets\logo-44x44.png")
    Copy-Item ./assets/logo-150x150.png (Join-Path $stage "assets\logo-150x150.png")

    $output = Join-Path $packages "WeekNumberIcon_${Version}_${architecture}.msix"
    if (Test-Path $output) { Remove-Item $output }
    & $makeappx pack /d $stage /p $output /o
    if ($LASTEXITCODE -ne 0) { throw "makeappx pack failed for $architecture." }
    if ($CertificateThumbprint) {
        & $signtool sign /fd SHA256 /sha1 $CertificateThumbprint $output
        if ($LASTEXITCODE -ne 0) { throw "signtool sign failed for $architecture." }
    }
    Write-Host "Created $output"
}

$bundle = Join-Path $root "zig-out\msix\WeekNumberIcon_${Version}.msixbundle"
if (Test-Path $bundle) { Remove-Item $bundle }
& $makeappx bundle /d $packages /p $bundle /bv $Version /o
if ($LASTEXITCODE -ne 0) { throw 'makeappx bundle failed.' }
if ($CertificateThumbprint) {
    & $signtool sign /fd SHA256 /sha1 $CertificateThumbprint $bundle
    if ($LASTEXITCODE -ne 0) { throw 'signtool sign failed for the bundle.' }
}
Write-Host "Created $bundle"
