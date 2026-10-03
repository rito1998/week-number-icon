# Week Number Icon

Display the current week number in the windows icons tray.

![img](./assets/showcase.png)

## Install

This app is available through the [Microsoft Store](https://apps.microsoft.com/detail/9P8Z35ZBQQKW?hl=en-us&gl=CH&ocid=pdpshare).

[![Get it from Microsoft](https://get.microsoft.com/images/en-us%20dark.svg)](https://get.microsoft.com/installer/download/9p8z35zbqqkw?referrer=appbadge)

Alternatively, install it with winget in PowerShell:

```pwsh
winget install --id 9P8Z35ZBQQKW --source msstore
```

## Build

### Build from source

```pwsh
zig build --release=safe
```

### Package and install MSIX

Package MSIX and install it from an elevated powershell:

```pwsh
./packaging/package.ps1
```

From an elevated PowerShell install the MSIX package:

```pwsh
Add-AppxPackage -Path '.\zig-out\msix\WeekNumberIcon_0.1.0.0.msixbundle' -AllowUnsigned
```

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.
