const std = @import("std");
const ArrayList = std.ArrayList;

const c = @cImport({
    @cInclude("lex/lex_strings.h");
    @cInclude("utils.h");
});

pub const CharClass = enum(c_int) {
    Eof = c.CH_EOF,
    Space = c.CH_SPACE,
    Alpha = c.CH_ALPHA,
    Digit = c.CH_DIGIT,
    Underscore = c.CH_UNDERSCORE,
    Symbol = c.CH_SYMBOL,

    // specific un-redefineable symbols
    Escape,
    LCurly,
    DQuote
};

pub export fn classify_char(ch: u8) CharClass {
    return switch (ch) {
        0x00 => .Eof,
        0x09, 0x0A, 0x0D, 0x20 => .Space,
        'a'...'z', 'A'...'Z' => .Alpha,
        '0'...'9' => .Digit,
        '_' => .Underscore,
        '`' => .Escape,
        '{' => .LCurly,
        '"' => .DQuote,
        else => if (ch < 0x20) {
            c.fungus_panic("cannot classify character 0x%hhx.\n", ch);
            unreachable;
        } else .Symbol
    };
}

// just exposing @tagName to c
export fn CharClass_name(class: CharClass) [*:0]const u8 {
    return @tagName(class);
}

pub fn escapeCStr(str: [*:0]const u8, len: usize) !ArrayList(u8) {
    var buf = ArrayList(u8).init(std.heap.c_allocator);

    // create escaped string
    var i: usize = 0;
    while (i < len) : (i += 1) {
        switch (str[i]) {
            0x00 => try buf.appendSlice("\\0"),
            '\n' => try buf.appendSlice("\\n"),
            '\r' => try buf.appendSlice("\\r"),
            '\t' => try buf.appendSlice("\\t"),
            '\\' => try buf.appendSlice("\\\\"),
            '\'' => try buf.appendSlice("\\'"),
            '\"' => try buf.appendSlice("\\\""),
            else => |ch| {
                if (ch < 0x20)
                    @panic("unimplemented");

                try buf.append(ch);
            }
        }
    }

    return buf;
}

// exposes escapeCStr to c
export fn escape_cstr(str: [*:0]const u8, len: usize) [*:0]u8 {
    var buf = escapeCStr(str, len)
        catch @panic("escapeCStr failed.");
    defer buf.deinit();

    // copy over escaped string memory
    const buf_len = buf.items.len;
    const raw_mem = std.c.malloc(buf_len + 1)
        orelse @panic("malloc failed.");
    var cstr = @ptrCast([*:0]u8, raw_mem);

    var i: usize = 0;
    while (i < buf_len) : (i += 1)
        cstr[i] = buf.items[i];

    cstr[buf_len] = 0;

    return cstr;
}