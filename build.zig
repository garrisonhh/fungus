const std = @import("std");
const mem = std.mem;
const stdout = std.io.getStdOut().writer();

pub fn build(b: *std.build.Builder) anyerror!void {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    const allocator = gpa.allocator();
    defer _ = gpa.deinit();

    const target = b.standardTargetOptions(.{});
    const mode = b.standardReleaseOptions();

    const exe = b.addExecutable("fungus", null);
    exe.setTarget(target);
    exe.setBuildMode(mode);
    exe.setOutputDir(b.pathFromRoot("."));
    exe.linkLibC();

    // walk all C sources and add to exe
    {
        const c_flags = .{
            "-Wall",
            "-Wextra",
            "-Wpedantic",
            "-Wvla",
            "-std=c11"
        };

        const abs_src_dir = b.pathFromRoot("src");
        var src_dir = try std.fs.openDirAbsolute(abs_src_dir,
                                                 .{ .iterate = true });
        defer src_dir.close();

        var sources = try src_dir.walk(allocator);
        defer sources.deinit();

        while (try sources.next()) |entry| {
            const path = b.pathJoin(&.{abs_src_dir, entry.path});

            if (mem.endsWith(u8, path, ".c"))
                exe.addCSourceFile(path, &c_flags);
        }
    }

    exe.install();

    // zig build run
    const run_cmd = exe.run();
    run_cmd.step.dependOn(b.getInstallStep());

    if (b.args) |args| {
        run_cmd.addArgs(args);
    }

    const run_step = b.step("run", "Run fungus!");
    run_step.dependOn(&run_cmd.step);
}
