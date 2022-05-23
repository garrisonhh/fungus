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
    Lexeme = c.TOK_LEXEME,
    Ident = c.TOK_IDENT,
    Bool = c.TOK_BOOL,
    Int = c.TOK_INT,
    Float = c.TOK_FLOAT,
    String = c.TOK_STRING,
    Escape = c.TOK_ESCAPE,
    Scope = c.TOK_SCOPE
};

// TODO once I can remove CTokBuf, pretty sure I can replace this entirely
// with a MultiArrayList
const TokBuf = struct {
    const INIT_CAP: usize = 64;

    types: []TokType,
    starts: []hsize_t,
    lens: []hsize_t,
    len: usize,

    const CTokBuf = extern struct {
        tbuf: *TokBuf,
        types: [*]TokType,
        starts: [*]hsize_t,
        lens: [*]hsize_t,
        len: usize
    };

    const Self = @This();

    fn asCTokBuf(self: *Self) CTokBuf {
        return CTokBuf{
            .tbuf = self,
            .types = self.*.types.ptr,
            .starts = self.*.starts.ptr,
            .lens = self.*.lens.ptr,
            .len = self.*.len
        };
    }

    fn init(self: *Self) Allocator.Error!void {
        self.types = try c_allocator.alloc(TokType, INIT_CAP);
        self.starts = try c_allocator.alloc(hsize_t, INIT_CAP);
        self.lens = try c_allocator.alloc(hsize_t, INIT_CAP);
        self.len = 0;
    }

    fn deinit(self: *Self) void {
        c_allocator.free(self.types);
        c_allocator.free(self.starts);
        c_allocator.free(self.lens);
    }

    fn doubleBufSize(buf: anytype) @TypeOf(buf) {
        return c_allocator.realloc(buf, buf.len * 2)
            catch @panic("couldn't resize buffer.");
    }

    fn emit(self: *Self, ty: TokType, start: hsize_t, len: hsize_t) void {
        if (self.len == self.types.len) {
            self.types = doubleBufSize(self.types);
            self.starts = doubleBufSize(self.starts);
            self.lens = doubleBufSize(self.lens);
        }

        self.types[self.len] = ty;
        self.starts[self.len] = start;
        self.lens[self.len] = len;
        self.len += 1;
    }

    fn peek(self: *Self) ?TokType {
        return if (self.types.len == 0)
            null
        else
            self.types[self.len - 1];
    }

    fn dump(self: *Self, file: *c.File) void {
        _ = c.printf(c.TC_CYAN ++ "Self:" ++ c.TC_RESET ++ "\n");

        var i: usize = 0;
        while (i < self.len) : (i += 1) {
            const color = switch (self.types[i]) {
                .Ident => c.TC_BLUE,
                .Bool, .Int, .Float => c.TC_MAGENTA,
                .String => c.TC_GREEN,
                else => c.TC_WHITE
            };
            const tag_name = @tagName(self.types[i]);

            _ = c.printf("%.*s: '%s%.*s" ++ c.TC_RESET ++ "'\n",
                         @intCast(c_int, tag_name.len),
                         tag_name.ptr,
                         color,
                         @intCast(c_int, self.lens[i]),
                         &c.File_str(file)[self.starts[i]]);
        }

        _ = c.printf("\n");
    }
};

const LexError = error {
    EmptyFile,
    SymbolNotFound,
    UnmatchedLCurly,
};

fn splitSymbol(
    tbuf: *TokBuf, lang: *c.Lang, slice: []const u8, start: hsize_t
) LexError!void {
    var i: hsize_t = 0;
    while (i < slice.len) {
        const token_view = c.View{
            .str = &slice[i],
            .len = slice.len - i,
        };
        const match_len =
            @intCast(hsize_t,
                     c.HashSet_longest(c.Lang_syms(lang), &token_view));

        if (match_len == 0) {
            return LexError.SymbolNotFound;
        }

        tbuf.emit(.Lexeme, start + i, match_len);
        i += match_len;
    }
}

// words can be lexemes or identifiers, this checks and adds the correct one
fn addWord(
    tbuf: *TokBuf, lang: *c.Lang, slice: []const u8, start: hsize_t
) LexError!void {
    const token_cword = c.Word_new(slice.ptr, slice.len);
    var word_type =
        if (std.mem.eql(u8, slice, "true") or std.mem.eql(u8, slice, "false"))
            TokType.Bool
        else if (c.HashSet_has(c.Lang_words(lang), &token_cword))
            TokType.Lexeme
        else
            TokType.Ident;

    tbuf.emit(word_type, start, @intCast(hsize_t, slice.len));
}

// TODO specific and descriptive user-facing errors
fn tokenize(
    tbuf: *TokBuf,
    file: *c.File,
    lang: *c.Lang,
    scope_start: usize,
    scope_len: usize
) LexError!void {
    const str = c.File_str(file)[scope_start..scope_start + scope_len];

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

                try addWord(tbuf, lang, str[start..i], start);
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
                if (tbuf.peek()) |last| {
                    if (last == .Escape) {
                        tbuf.emit(.Lexeme, start, i - start);
                        break :blk;
                    }
                }

                // lexemes that aren't literals must be split into valid lexemes
                splitSymbol(tbuf, lang, str[start..i], start) catch |e| {
                    c.File_error_at(file, start, i - start,
                                    "symbol not found.");
                    return e;
                };
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

                tbuf.emit(tok_type, start, i - start);
            },
            .Escape => {
                tbuf.emit(.Escape, i, 1);
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

                tbuf.emit(.String, start, i - start);
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
                    c.File_error_from(file, start, "unmatched curly.");
                    return LexError.UnmatchedLCurly;
                }

                tbuf.emit(.Scope, start, i - start);
            }
        }
    }
}

// c interface =================================================================

export fn TokBuf_new() TokBuf.CTokBuf {
    var tbuf: TokBuf = undefined;
    tbuf.init() catch |e| utils.reportErrorAndPanic(e);

    return tbuf.asCTokBuf();
}

export fn TokBuf_del(ctbuf: *TokBuf.CTokBuf) void {
    ctbuf.tbuf.deinit();
    c_allocator.destroy(ctbuf.tbuf);
}

export fn lex(
    file: *c.File, lang: *c.Lang, start: usize, len: usize
) TokBuf.CTokBuf {
    var tbuf = c_allocator.create(TokBuf) catch c.abort();
    tbuf.init() catch c.abort();

    // TODO a more descriptive error report, and don't panic
    tokenize(tbuf, file, lang, start, len)
        catch |e| utils.reportErrorAndPanic(e);

    return tbuf.asCTokBuf();
}

export fn TokBuf_dump(ctbuf: *TokBuf.CTokBuf, file: *c.File) void {
    ctbuf.tbuf.dump(file);
}