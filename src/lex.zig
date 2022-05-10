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
    Word = c.TOK_WORD,
    Symbol = c.TOK_SYMBOL,
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

    fn push(self: *TokBuf, ty: TokType, start: hsize_t, len: hsize_t) void {
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
            self.*.types[self.*.types.len - 1];
    }

    fn dump(self: *TokBuf, file: *c.File) void {
        _ = c.printf(c.TC_CYAN ++ "TokBuf:" ++ c.TC_RESET ++ "\n");

        var i: usize = 0;
        while (i < self.*.len) : (i += 1) {
            const color = switch (self.*.types[i]) {
                .Word => c.TC_BLUE,
                .Bool, .Int, .Float => c.TC_MAGENTA,
                .String => c.TC_GREEN,
                else => c.TC_WHITE
            };

            _ = c.printf("%s%.*s" ++ c.TC_RESET ++ "\n", color,
                         @intCast(c_int, self.*.lens[i]),
                         &c.File_str(file)[self.*.starts[i]]);
        }

        _ = c.printf("\n");
    }
};

fn splitSymbol(
    tbuf: *TokBuf, lang: *c.Lang, slice: []const u8, start: hsize_t
) void {
    var i: hsize_t = 0;
    while (i < slice.len) {
        const token_view = c.View{
            .str = &slice[i],
            .len = slice.len - i,
        };
        const match_len =
            @intCast(hsize_t, c.HashSet_longest(c.Lang_syms(lang), &token_view));

        if (match_len == 0) {
            _ = c.printf("couldn't find symbol '%.*s'\n",
                         @intCast(c_int, token_view.len), &slice[i]);
            _ = c.printf("in: ");
            c.HashSet_print(c.Lang_syms(lang));
            _ = c.puts("");

            // TODO return error
            @panic("no symbol match");
        }

        tbuf.push(.Symbol, start + i, match_len);
        i += match_len;
    }
}

// TODO specific and descriptive errors
fn tokenize(tbuf: *TokBuf, file: *c.File, lang: *c.Lang) anyerror!void {
    const str: []const u8 = c.File_str(file)[0..c.File_len(file) + 1];

    var i: hsize_t = 0;
    while (i < str.len) {
        var class: CharClass = undefined;

        // skip whitespace
        while (true) : (i += 1) {
            class = classifyChar(str[i]);

            if (class != .Space)
                break;
        }

        // identify next token
        switch (class) {
            .Eof => break,
            .Space => unreachable,
            .Alpha, .Underscore => {
                // word
                const start = i;

                while (true) {
                    class = classifyChar(str[i]);

                    switch (class) {
                        .Alpha, .Underscore, .Digit => i += 1,
                        else => break
                    }
                }

                tbuf.push(.Word, start, i - start);
            },
            .Symbol => {
                // symbols
                const start = i;

                while (true) : (i += 1) {
                    class = classifyChar(str[i]);

                    if (class != .Symbol)
                        break;
                }

                blk: {
                    // check for literal lexeme
                    if (tbuf.peek()) |last| {
                        if (last == .Escape)
                            tbuf.push(.Symbol, start, i - start);

                        break :blk;
                    }

                    splitSymbol(tbuf, lang, str[start..i], start);
                }
            },
            .Digit => {
                // ints and floats
                const start = i;
                var tok_type: TokType = .Int;

                while (true) : (i += 1) {
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

                tbuf.push(tok_type, start, i - start);
            },
            .Escape => {
                tbuf.push(.Escape, i, i + 1);
                i += 1;
            },
            .DQuote => @panic("TODO strings"),
            .LCurly, .RCurly => @panic("TODO scopess"),
        }
    }

    // TODO REMOVE vvv
    tbuf.dump(file);
    c.exit(0);
}

export fn lex(file: *c.File, lang: *c.Lang) TokBuf.CTokBuf {
    var tbuf = c_allocator.create(TokBuf) catch c.abort();
    tbuf.init() catch c.abort();

    tokenize(tbuf, file, lang) catch {
        // TODO descriptive error report
        c.fungus_panic("tokenization failed.");
    };

    tbuf.dump(file);

    _ = c.printf("");
    c.exit(-1);

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