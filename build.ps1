$targets = @(
    "x86_64-windows"
    "x86-windows"
    "aarch64-windows"
)

foreach ($tgt in $targets) {
    # Build binaries:
    #   zig-out\x86_64-windows\bin\week-number-icon.exe
    #   zig-out\x86-windows\bin\week-number-icon.exe
    #   zig-out\aarch64-windows\bin\week-number-icon.exe

    zig build -Dtarget="$tgt" --release=safe --prefix "zig-out/$tgt"
}
