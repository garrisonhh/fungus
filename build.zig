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
        var c_flags = std.ArrayList([]const u8).init(allocator);
        defer c_flags.deinit();

        try c_flags.appendSlice(&.{
            "-Wall",
            "-Wextra",
            "-Wpedantic",
            "-Wvla",
            "-std=c11"
        });

        if (mode == std.builtin.Mode.Debug) {
            try c_flags.append("-DDEBUG");
            try c_flags.append("-ggdb");
        } else {
            try c_flags.append("-DNDEBUG");
            try c_flags.append("-O3");
        }

        const abs_src_dir = b.pathFromRoot("src");

        if (false) {
            // full source build
            var src_dir = try std.fs.openDirAbsolute(abs_src_dir,
                                                     .{ .iterate = true });
            defer src_dir.close();

            var sources = try src_dir.walk(allocator);
            defer sources.deinit();

            while (try sources.next()) |entry| {
                const path = b.pathJoin(&.{abs_src_dir, entry.path});

                if (mem.endsWith(u8, path, ".c"))
                    exe.addCSourceFile(path, c_flags.items);
            }
        } else {
            // current rewrite build
            const sources = [_][]const u8 {
                "main.c",

                // lexical analysis
                "lex.c",

                // parsing
                "parse.c",
                "lang.c",
                "lang/rules.c",
                "lang/precedence.c",
                "lang/pattern.c",
                "lang/fungus.c",

                // utility
                "file.c",
                "utils.c",
                "data.c",
            };

            for (sources) |source| {
                const path = b.pathJoin(&.{abs_src_dir, source});

                exe.addCSourceFile(path, c_flags.items);
            }
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
