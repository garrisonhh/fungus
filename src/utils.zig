const std = @import("std");

const c = @cImport({
    @cInclude("utils.h");
});

pub fn reportErrorAndPanic(e: anyerror) noreturn {
    c.fungus_panic("failed with error %s", @errorName(e).ptr);
    unreachable;
}

/// `fungus_panic`s on error. only for things that would invalidate the internal
/// state of fungus.
pub fn must(value: anytype) @typeInfo(@TypeOf(value)).ErrorUnion.payload {
    std.debug.assert(@typeInfo(@TypeOf(value)) == .ErrorUnion);
    return value catch |e| reportErrorAndPanic(e);
}

pub fn ptrCastAligned(comptime T: type, ptr: anytype) T {
    std.debug.assert(@typeInfo(T) == .Pointer);

    comptime return @ptrCast(T, @alignCast(@alignOf(T), ptr));
}

/// submit an enum type, returns an array of strings with names in lowercase
/// names include a nul terminator for c interop
pub fn lowerCaseEnumTags(comptime T: type) [][]u8 {
    comptime {
        const info = @typeInfo(T);
        const fields = info.Enum.fields;

        // find longest field
        var longest_field: usize = 0;
        for (fields) |field| {
            if (field.name.len > longest_field) {
                longest_field = field.name.len;
            }
        }

        // write field names to a buffer
        var buf: [fields.len][longest_field + 1]u8 = undefined;
        var output: [buf.len][]u8 = undefined;

        for (fields) |field, i| {
            std.mem.copy(u8, buf[i][0..], field.name);
            buf[i][field.name.len] = 0;
            output[i] = buf[i][0..field.name.len];

            for (output[i]) |ch, j| {
                if (ch >= 'A' and ch <= 'Z') {
                    output[i][j] = ch + 'a' - 'A';
                }
            }
        }

        return output[0..];
    }
}