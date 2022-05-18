const std = @import("std");
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

    fn asCTokBuf(self: *TokBuf) CTokBuf {
        return CTokBuf{
            .tbuf = self,
            .types = self.*.types.ptr,
            .starts = self.*.starts.ptr,
            .lens = self.*.lens.ptr,
            .len = self.*.len
        };
    }

    fn init(self: *TokBuf) Allocator.Error!void {
        self.*.types = try c_allocator.alloc(TokType, INIT_CAP);
        self.*.starts = try c_allocator.alloc(hsize_t, INIT_CAP);
        self.*.lens = try c_allocator.alloc(hsize_t, INIT_CAP);
        self.*.len = 0;
    }

    fn deinit(self: *TokBuf) void {
        c_allocator.free(self.*.types);
        c_allocator.free(self.*.starts);
        c_allocator.free(self.*.lens);
    }

    fn doubleBufSize(buf: anytype) @TypeOf(buf) {
        return c_allocator.realloc(buf, buf.len * 2)
            catch @panic("couldn't resize buffer.");
    }

    fn emit(self: *TokBuf, ty: TokType, start: hsize_t, len: hsize_t) void {
        if (self.*.len == self.*.types.len) {
            self.*.types = doubleBufSize(self.*.types);
            self.*.starts = doubleBufSize(self.*.starts);
            self.*.lens = doubleBufSize(self.*.lens);
        }

        self.*.types[self.*.len] = ty;
        self.*.starts[self.*.len] = start;
        self.*.lens[self.*.len] = len;
        self.*.len += 1;
    }

    fn peek(self: *TokBuf) ?TokType {
        return if (self.*.types.len == 0)
            null
        else
            self.*.types[self.*.len - 1];
    }

    fn dump(self: *TokBuf, file: *c.File) void {
        _ = c.printf(c.TC_CYAN ++ "TokBuf:" ++ c.TC_RESET ++ "\n");

        var i: usize = 0;
        while (i < self.*.len) : (i += 1) {
            const color = switch (self.*.types[i]) {
                .Ident => c.TC_BLUE,
                .Bool, .Int, .Float => c.TC_MAGENTA,
                .String => c.TC_GREEN,
                else => c.TC_WHITE
            };
            const tag_name = @tagName(self.*.types[i]);

            _ = c.printf("%.*s: '%s%.*s" ++ c.TC_RESET ++ "'\n",
                         @intCast(c_int, tag_name.len),
                         tag_name.ptr,
                         color,
                         @intCast(c_int, self.*.lens[i]),
                         &c.File_str(file)[self.*.starts[i]]);
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

        if (match_len == 0)
            return LexError.SymbolNotFound;

        tbuf.emit(.Lexeme, start + i, match_len);
        i += match_len;
    }
}

// words can be lexemes or identifiers, this checks and adds the correct one
fn addWord(
    tbuf: *TokBuf, lang: *c.Lang, slice: []const u8, start: hsize_t
) LexError!void {
    const token_cword = c.Word_new(slice.ptr, slice.len);
    var word_type: TokType =
        if (c.HashSet_has(c.Lang_words(lang), &token_cword))
            .Lexeme
        else
            .Ident;

    tbuf.emit(word_type, start, @intCast(hsize_t, slice.len));
}

// TODO specific and descriptive errors
fn tokenize(tbuf: *TokBuf, file: *c.File, lang: *c.Lang) LexError!void {
    if (c.File_len(file) == 0) {
        return LexError.EmptyFile;
    }

    const str: []const u8 = c.File_str(file)[0..c.File_len(file)];

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

                try splitSymbol(tbuf, lang, str[start..i], start);
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

                while (i < str.len) {
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

                    if (level == 0)
                        break;
                } else {
                    return LexError.UnmatchedLCurly;
                }

                tbuf.emit(.Scope, start, i - start);
            }
        }
    }
}

export fn lex(file: *c.File, lang: *c.Lang) TokBuf.CTokBuf {
    var tbuf = c_allocator.create(TokBuf) catch c.abort();
    tbuf.init() catch c.abort();

    tokenize(tbuf, file, lang) catch |e| {
        // TODO descriptive error report
        c.fungus_panic("tokenization failed with error %s", @errorName(e).ptr);
    };

    return tbuf.asCTokBuf();
}

export fn TokBuf_del(ctbuf: *TokBuf.CTokBuf) void {
    const tbuf = ctbuf.tbuf;

    tbuf.deinit();
    c_allocator.destroy(tbuf);
}

export fn TokBuf_dump(ctbuf: *TokBuf.CTokBuf, file: *c.File) void {
    ctbuf.tbuf.dump(file);
}