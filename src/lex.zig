const std = @import("std");
const utils = @import("utils.zig");
const Allocator = std.mem.Allocator;
const c_allocator = std.heap.c_allocator;

const lex_strings = @import("lex/lex_strings.zig");
const classifyChar = lex_strings.classify_char;
const CharClass = lex_strings.CharClass;

const c = @cImport({
    @cInclude("lang.h");
    @cInclude("lex.h");
    @cInclude("utils.h");
    @cInclude("data.h");
});

const hsize_t = c.hsize_t;

const TokType = enum(c_int) {
    Invalid = c.TOK_INVALID,
    Lexeme  = c.TOK_LEXEME,
    Ident   = c.TOK_IDENT,
    Bool    = c.TOK_BOOL,
    Int     = c.TOK_INT,
    Float   = c.TOK_FLOAT,
    String  = c.TOK_STRING,
    Escape  = c.TOK_ESCAPE,
    Scope   = c.TOK_SCOPE
};

// TODO once I can remove CTokBuf, pretty sure I can replace this entirely
// with a MultiArrayList
const TokBuf = struct {
    const allocator = c_allocator;

    tokens: std.MultiArrayList(struct {
        toktype: TokType,
        start: hsize_t,
        len: hsize_t,
    }),

    const Self = @This();

    pub fn init(self: *Self) void {
        self.tokens = @TypeOf(self.tokens){};
    }

    pub fn deinit(self: *Self) void {
        self.tokens.deinit(Self.allocator);
    }

    pub fn emit(self: *Self, ty: TokType, start: hsize_t, len: hsize_t) !void {
        try self.tokens.append(Self.allocator, .{
            .toktype = ty,
            .start = start,
            .len = len
        });
    }

    pub fn peek(self: *Self) ?TokType {
        return if (self.tokens.len == 0)
            null
        else
            self.tokens.get(self.tokens.len - 1).toktype;
    }

    pub fn dump(self: *Self, file: *c.File) void {
        _ = c.printf(c.TC_CYAN ++ "Self:" ++ c.TC_RESET ++ "\n");

        var i: usize = 0;
        while (i < self.tokens.len) : (i += 1) {
            const token = self.tokens.get(i);

            const color = switch (token.toktype) {
                .Ident => c.TC_BLUE,
                .Bool, .Int, .Float => c.TC_MAGENTA,
                .String => c.TC_GREEN,
                else => c.TC_WHITE
            };
            const tag_name = @tagName(token.toktype);

            _ = c.printf("%.*s: '%s%.*s" ++ c.TC_RESET ++ "'\n",
                         @intCast(c_int, tag_name.len),
                         tag_name.ptr,
                         color,
                         @intCast(c_int, token.len),
                         &c.File_str(file)[token.start]);
        }

        _ = c.printf("\n");
    }
};

/// syntax error displaying region of a file
fn lexErrorAt(
    file: *c.File, start: hsize_t, len: hsize_t, msg: []const u8
) noreturn {
    c.File_error_at(file, start, len, msg.ptr);
    std.os.exit(255);
}

/// syntax error displaying start of a region of a file
fn lexErrorFrom(file: *c.File, start: hsize_t, msg: []const u8) noreturn {
    c.File_error_from(file, start, msg.ptr);
    std.os.exit(255);
}

fn splitSymbol(ctx: LexContext, start: hsize_t, len: hsize_t) !void {
    var i: hsize_t = 0;
    while (i < len) {
        const token_view = c.View{
            .str = &c.File_str(ctx.file)[start + i],
            .len = len - i,
        };
        const match_len =
            @intCast(hsize_t,
                     c.HashSet_longest(c.Lang_syms(ctx.lang), &token_view));

        if (match_len == 0) {
            lexErrorAt(ctx.file, start + i, len - i, "unknown symbol");
        }

        try ctx.tbuf.emit(.Lexeme, start + i, match_len);
        i += match_len;
    }
}

// words can be lexemes or identifiers, this checks and adds the correct one
fn addWord(ctx: LexContext, slice: []const u8, start: hsize_t) !void {
    const token_cword = c.Word_new(slice.ptr, slice.len);
    var word_type =
        if (std.mem.eql(u8, slice, "true") or std.mem.eql(u8, slice, "false"))
            TokType.Bool
        else if (c.HashSet_has(c.Lang_words(ctx.lang), &token_cword))
            TokType.Lexeme
        else
            TokType.Ident;

    try ctx.tbuf.emit(word_type, start, @intCast(hsize_t, slice.len));
}

const LexContext = struct {
    tbuf: *TokBuf,
    file: *c.File,
    lang: *c.Lang,
};

fn tokenize(ctx: LexContext, scope_start: usize, scope_len: usize) !void {
    const str = c.File_str(ctx.file)[scope_start..scope_start + scope_len];

    var i: hsize_t = 0;
    while (true) {
        var class: CharClass = undefined;

        // skip whitespace
        while (i < str.len) : (i += 1) {
            class = classifyChar(str[i]);

            if (class != .Space)
                break;
        } else {
            break;
        }

        // identify next token
        switch (class) {
            .Eof => break,
            .Space => unreachable,
            .Alpha, .Underscore => {
                // word
                const start = i;

                while (i < str.len) {
                    class = classifyChar(str[i]);

                    switch (class) {
                        .Alpha, .Underscore, .Digit => i += 1,
                        else => break
                    }
                }

                try addWord(ctx, str[start..i], start);
            },
            .Symbol => blk: {
                // symbols
                const start = i;

                while (i < str.len) : (i += 1) {
                    class = classifyChar(str[i]);

                    if (class != .Symbol)
                        break;
                }

                // check for literal lexeme
                if (ctx.tbuf.peek()) |last| {
                    if (last == .Escape) {
                        try ctx.tbuf.emit(.Lexeme, start, i - start);
                        break :blk;
                    }
                }

                // lexemes that aren't literals must be split into valid lexemes
                try splitSymbol(ctx, start, i - start);
            },
            .Digit => {
                // ints and floats
                const start = i;
                var tok_type: TokType = .Int;

                while (i < str.len) : (i += 1) {
                    class = classifyChar(str[i]);

                    if (class != .Digit and class != .Underscore)
                        break;
                }

                if (class == .Symbol and str[i] == '.') {
                    tok_type = .Float;
                    i += 1;

                    while (true) : (i += 1) {
                        class = classifyChar(str[i]);

                        if (class != .Digit and class != .Underscore)
                            break;
                    }
                }

                try ctx.tbuf.emit(tok_type, start, i - start);
            },
            .Escape => {
                try ctx.tbuf.emit(.Escape, i, 1);
                i += 1;
            },
            .DQuote => {
                // strings
                const start = i;

                while (i < str.len - 1) {
                    i += 1;

                    if (str[i] == '"' and str[i - 1] != '\\')
                        break;
                }

                try ctx.tbuf.emit(.String, start, i - start);
            },
            .LCurly => {
                // scopes
                const start = i;

                var level: i32 = 0;
                while (i < str.len) : (i += 1) {
                    switch (str[i]) {
                        '{' => level += 1,
                        '}' => level -= 1,
                        else => {}
                    }

                    if (level == 0) {
                        i += 1;
                        break;
                    }
                } else {
                    lexErrorFrom(ctx.file, start, "unmatched curly.");
                }

                try ctx.tbuf.emit(.Scope, start, i - start);
            }
        }
    }
}

// c interface =================================================================

export fn TokBuf_new() *TokBuf {
    var tbuf = utils.must(c_allocator.create(TokBuf));
    tbuf.init();

    return tbuf;
}

export fn TokBuf_del(tbuf: *TokBuf) void {
    tbuf.deinit();
    c_allocator.destroy(tbuf);
}

export fn TokBuf_len(tbuf: *TokBuf) usize {
    return tbuf.tokens.len;
}

export fn TokBuf_get(tbuf: *TokBuf, index: usize) c.Token {
    const data = tbuf.tokens.get(index);

    return c.Token{
        .@"type" = @intCast(c.TokType, @enumToInt(data.toktype)),
        .start = data.start,
        .len = data.len
    };
}

export fn lex(
    tbuf: *TokBuf,
    file: *c.File,
    lang: *c.Lang,
    start: usize,
    len: usize
) bool {
    if (len == 0) {
        return true;
    }

    const ctx = LexContext{
        .tbuf = tbuf,
        .file = file,
        .lang = lang
    };

    tokenize(ctx, start, len) catch |e| {
        _ = c.printf("tokenization failed with error %s.\n",
                     @errorName(e).ptr);

        return false;
    };

    return true;
}

export fn TokBuf_dump(tbuf: *TokBuf, file: *c.File) void {
    tbuf.dump(file);
}