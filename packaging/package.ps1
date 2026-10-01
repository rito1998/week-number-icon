param(
    [ValidatePattern('^\d+\.\d+\.\d+\.\d+$')][string]$Version,
    [string]$CertificateThumbprint,
    # Values reserved in Partner Center; the Store signs the uploaded bundle itself.
    [string]$StorePublisher,
    [string]$IdentityName,
    [string]$PublisherDisplayName,
    [string]$DisplayName
)

$ErrorActionPreference = 'Stop'
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

# Zig target triple -> MSIX ProcessorArchitecture
$targets = [ordered]@{
    'x86_64-windows'  = 'x64'
    'x86-windows'     = 'x86'
    'aarch64-windows' = 'arm64'
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

# The OID marks the package as installable via Add-AppxPackage -AllowUnsigned; Store packages must not carry it.
$publisher = 'CN=Riccardo Torreggiani, OID.2.25.311729368913984317654407730594956997722=1'
if ($StorePublisher) {
    $publisher = $StorePublisher
}
elseif ($CertificateThumbprint) {
    $certificate = Get-Item "Cert:\CurrentUser\My\$CertificateThumbprint" -ErrorAction Stop
    if (-not $certificate.HasPrivateKey) { throw 'Signing certificate needs a private key.' }
    $publisher = $certificate.Subject
}

function New-Logo {
    param([string]$Path, [int]$Size)

    $bitmap = [Drawing.Bitmap]::new($Size, $Size)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    $background = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(23, 92, 105))
    $foreground = [Drawing.SolidBrush]::new([Drawing.Color]::White)
    $font = [Drawing.Font]::new('Segoe UI', ($Size * 0.55), [Drawing.FontStyle]::Bold, [Drawing.GraphicsUnit]::Pixel)
    try {
        $graphics.Clear([Drawing.Color]::Transparent)
        $graphics.FillRectangle($background, 0, 0, $Size, $Size)
        $format = [Drawing.StringFormat]::new()
        try {
            $format.Alignment = [Drawing.StringAlignment]::Center
            $format.LineAlignment = [Drawing.StringAlignment]::Center
            $graphics.DrawString('W', $font, $foreground, [Drawing.RectangleF]::new(0, 0, $Size, $Size), $format)
        } finally { $format.Dispose() }
        $bitmap.Save($Path, [Drawing.Imaging.ImageFormat]::Png)
    } finally {
        $font.Dispose()
        $foreground.Dispose()
        $background.Dispose()
        $graphics.Dispose()
        $bitmap.Dispose()
    }
}

Add-Type -AssemblyName System.Drawing

# makeappx bundle requires a folder containing only the packages to bundle.
$packages = Join-Path $root 'zig-out\msix\packages'
if (Test-Path $packages) { Remove-Item $packages -Recurse -Force }
New-Item -ItemType Directory -Force $packages | Out-Null

foreach ($target in $targets.GetEnumerator()) {
    $architecture = $target.Value
    $exe = Join-Path $root "zig-out\$($target.Key)\bin\$exeName"
    if (-not (Test-Path $exe)) { throw "Build the executables first (build.ps1): $exe" }

    $stage = Join-Path $root "zig-out\msix\stage\$architecture"
    if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
    New-Item -ItemType Directory -Force (Join-Path $stage 'Assets') | Out-Null
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

    foreach ($size in 44, 150) {
        New-Logo -Path (Join-Path $stage "Assets\Square${size}x${size}Logo.png") -Size $size
    }

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
