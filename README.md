# Week Number Icon

A tiny Windows tray app that shows the current **ISO week number** in the notification area.

[![CI](https://github.com/rito1998/week-number-icon/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/rito1998/week-number-icon/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

![The Week Number Icon showing the current week in the Windows system tray](./assets/showcase.png)

## Install

This app is available through the [Microsoft Store](https://apps.microsoft.com/detail/9P8Z35ZBQQKW?hl=en-us&gl=CH&ocid=pdpshare).

[![Get it from Microsoft](https://get.microsoft.com/images/en-us%20dark.svg)](https://get.microsoft.com/installer/download/9p8z35zbqqkw?referrer=appbadge)

Alternatively, install it with winget in PowerShell CLI:

```pwsh
winget install --id 9P8Z35ZBQQKW --source msstore
```

## Build

Requires [Zig](https://ziglang.org/). Building the MSIX
package additionally needs `makeappx.exe` from the Windows SDK.

```pwsh
zig build --release=safe
```

### Package MSIX

```pwsh
zig build msix --release=safe
```

For local testing, install the unsigned msixbundle from an elevated PowerShell:

```pwsh
Add-AppxPackage -Path '.\zig-out\msix\*-unsigned.msixbundle' -AllowUnsigned
```

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.
