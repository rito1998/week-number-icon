# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.2.0] - 2026-10-28

### Added

- Color settings in the tray menu: six presets (Default, White on black, Black on
  white, Amber on black, Green on black, Blue on white).
- Custom color dialog: hex input accepting `#RRGGBB`, `RRGGBB`, `0xRRGGBB` and the
  `#RGB` shorthand, a standard Windows color picker (`Pick...`) and a live preview
  swatch; invalid values are rejected with a hint instead of closing the dialog.
- Contrast guard: text and background colors that are too similar to read are refused.
- Settings persistence under `HKCU\Software\WeekNumberIcon`, restored on every start.
- `hexcolor` parsing/formatting module with unit tests, runnable via `zig build test`.

### Changed

- The icon repaints immediately when a color changes (on top of the existing
  once-a-minute refresh).

## [0.1.1] - 2026-10-09

### Fixed

- MSIX package naming for Microsoft Store submission.

### Changed

- The build always produces both regular and `-unsigned` MSIX bundles.
- CI jobs simplified; Zig setup action upgraded; Zig 0.17.0 baseline.

## [0.1.0] - 2026-10-01

### Added

- Initial release: ISO week number in the Windows notification area, refreshed
  automatically every minute.
- x86 / x64 / ARM64 builds and MSIX packaging through the Zig build system.
- Microsoft Store listing and `winget` installation.
