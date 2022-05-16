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
    exe.setOutputDir(".");
    exe.linkLibC();

    const src_dir = b.pathFromRoot("src");

    // zig sources (compiled as separate object and linked with C source)
    {
        const zig_sources = [_][2][]const u8{
            // .{ "char_classify", "lex/char_classify.zig" },
            .{ "lex", "lex.zig" },
        };

        for (zig_sources) |name_and_path| {
            const name = name_and_path[0];
            const path = name_and_path[1];
            const obj = b.addObject(name, b.pathJoin(&.{src_dir, path}));

            obj.linkLibC();
            obj.addIncludeDir("src");

            exe.addObject(obj);
        }
    }

    // c sources
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
            try c_flags.append("-O0");
            // ubsan makes it impossible to debug segfaults w/ valgrind
            try c_flags.append("-fno-sanitize=undefined");
        } else {
            try c_flags.append("-DNDEBUG");
            try c_flags.append("-O3");
        }

        const c_sources = [_][]const u8 {
            "main.c",
            "fungus.c",

            // lexical analysis
            //"lex.c",

            // parsing
            "parse.c",
            "lang.c",
            "lang/rules.c",
            "lang/precedence.c",
            "lang/pattern.c",
            "lang/ast_expr.c",

            // sema
            "sema.c",
            "sema/types.c",
            "sema/names.c",

            // sir
            "sir.c",

            // utility
            "file.c",
            "utils.c",
            "data.c",
        };

        for (c_sources) |source| {
            const path = b.pathJoin(&.{src_dir, source});

            exe.addCSourceFile(path, c_flags.items);
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
