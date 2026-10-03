const std = @import("std");
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

    exe.root_module.linkSystemLibrary("user32", .{});
    exe.root_module.linkSystemLibrary("shell32", .{});
    exe.root_module.linkSystemLibrary("gdi32", .{});
    exe.subsystem = .windows; // necessary together with "-mwindows" to prevent showing console

    const install_artifact = b.addInstallArtifact(exe, .{
        .dest_dir = .{ .override = .{ .custom = @tagName(target.result.cpu.arch) } },
    });

    b.default_step.dependOn(&install_artifact.step);
}
