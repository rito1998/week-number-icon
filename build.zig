const std = @import("std");
const version: []const u8 = @import("build.zig.zon").version;
const ResolvedTarget = std.Build.ResolvedTarget;

pub fn build(b: *std.Build) void {
    const optimize = b.standardOptimizeOption(.{});

    const targets = [_]ResolvedTarget{
        b.resolveTargetQuery(.{ .cpu_arch = .x86_64, .os_tag = .windows }),
        b.resolveTargetQuery(.{ .cpu_arch = .x86, .os_tag = .windows }),
        b.resolveTargetQuery(.{ .cpu_arch = .aarch64, .os_tag = .windows }),
    };

    for (targets) |target| {
        compileAndInstall(b, target, optimize);
    }

    const msix_step = b.step("msix", "Build MSIX package");
    const package = b.addSystemCommand(&[_][]const u8{
        "powershell",
        "-ExecutionPolicy",
        "Bypass",
        "-File",
        b.path("packaging/package.ps1").getDisplayName(),
        "-Version",
        version ++ ".0",
    });
    package.step.dependOn(b.default_step);
    msix_step.dependOn(&package.step);

    addTests(b);
}

fn addTests(b: *std.Build) void {
    const test_step = b.step("test", "Run unit tests");

    const host = b.standardTargetOptions(.{});

    const hex_tests = b.addExecutable(.{
        .name = "hexcolor-test",
        .root_module = b.createModule(.{
            .target = host,
            .optimize = .Debug,
            .link_libc = true,
        }),
    });

    hex_tests.root_module.addCSourceFile(.{
        .file = b.path("src/hexcolor.c"),
        .flags = &[_][]const u8{},
    });

    hex_tests.root_module.addCSourceFile(.{
        .file = b.path("tests/hexcolor_test.c"),
        .flags = &[_][]const u8{},
    });

    const run_hex_tests = b.addRunArtifact(hex_tests);
    test_step.dependOn(&run_hex_tests.step);
}

fn compileAndInstall(b: *std.Build, target: ResolvedTarget, optimize: std.builtin.Optimize) void {
    const exe = b.addExecutable(.{
        .name = "week-number-icon",
        .root_module = b.createModule(.{
            .target = target,
            .optimize = optimize,
            .link_libc = true,
        }),
    });

    exe.root_module.addCSourceFile(.{
        .file = b.path("src/main.c"),
        .flags = &[_][]const u8{
            "-mwindows",
        },
    });

    exe.root_module.addCSourceFile(.{
        .file = b.path("src/hexcolor.c"),
        .flags = &[_][]const u8{},
    });

    exe.root_module.linkSystemLibrary("user32", .{});
    exe.root_module.linkSystemLibrary("shell32", .{});
    exe.root_module.linkSystemLibrary("gdi32", .{});
    exe.root_module.linkSystemLibrary("advapi32", .{}); // registry settings
    exe.root_module.linkSystemLibrary("comdlg32", .{}); // ChooseColorW
    exe.subsystem = .windows; // necessary together with "-mwindows" to prevent showing console

    const install_artifact = b.addInstallArtifact(exe, .{
        .dest_dir = .{ .override = .{ .custom = @tagName(target.result.cpu.arch) } },
    });

    b.default_step.dependOn(&install_artifact.step);
}
