# Week Number Icon

Display the current week number in the windows icons tray.

![img](./assets/showcase.png)

## Install

This app is available through the [Microsoft Store](https://apps.microsoft.com/detail/9P8Z35ZBQQKW?hl=en-us&gl=CH&ocid=pdpshare).

[![Get it from Microsoft](https://get.microsoft.com/images/en-us%20dark.svg)](https://get.microsoft.com/installer/download/9p8z35zbqqkw?referrer=appbadge)

## Build and install locally

Build from source and package msix:

```pwsh
./build.ps1
./packaging/package.ps1
```

Install app from the msix from an elevated powershell run:

```pwsh
Add-AppxPackage -Path '.\zig-out\msix\WeekNumberIcon_0.1.0.0.msixbundle' -AllowUnsigned
```

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.
