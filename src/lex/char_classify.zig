const std = @import("std");

const c = @cImport({
    @cInclude("lex/char_classify.h");
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
    RCurly,
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
        '}' => .RCurly,
        '"' => .DQuote,
        else =>  if (ch < 0x20) {
            c.fungus_panic("cannot classify character 0x%hhx.\n", ch);
            unreachable;
        } else .Symbol
    };
}

// just exposing @tagName to c
export fn CharClass_name(class: CharClass) [*:0]const u8 {
    return @tagName(class);
}