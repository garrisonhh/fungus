const std = @import("std");

const c = @cImport({
    @cInclude("fungus.h");
    @cInclude("lex.h"); // TODO just use the zig interface
    @cInclude("parse.h");
    @cInclude("sema.h");
    @cInclude("fir.h"); // TODO just use the zig interface
});

/// implements compiling files for main.c
export fn try_compile_file(file: *c.File, names: *c.Names) bool {
    _ = names;

    // lex
    var tokbuf = c.TokBuf_new();
    defer c.TokBuf_del(tokbuf);

    // TODO return an error from zig fn
    if (!c.lex(tokbuf, file, &c.fungus_lang, 0, file.*.text.len)) {
        @panic("lex error ?");
        // return false;
    }

    if (true) {
        _ = c.puts(c.TC_CYAN ++ "generated tokens:" ++ c.TC_RESET);
        c.TokBuf_dump(tokbuf, file);
    }

    // parse
    var parse_pool = c.Bump_new();
    defer c.Bump_del(&parse_pool);

    var ast_ctx = c.AstCtx{
        .pool = &parse_pool,
        .file = file,
        .lang = &c.fungus_lang,
    };

    const ast = c.parse(&ast_ctx, tokbuf);

    if (c.global_error) @panic("global_error set"); // TODO ew

    _ = ast;

    // sema
    c.sema(&c.SemaCtx{
        .pool = &parse_pool,
        .file = file,
        .lang = &c.fungus_lang,
        .names = names
    }, ast);

    if (c.global_error) @panic("global_error set"); // TODO ew

    if (true) {
        _ = c.puts(c.TC_CYAN ++ "generated ast:" ++ c.TC_RESET);
        c.AstExpr_dump(ast, &c.fungus_lang, file);
        _ = c.puts("");
    }

    // fir
    var fir_pool = c.Bump_new();
    defer c.Bump_del(&fir_pool);

    const fir = c.gen_fir(&fir_pool, file, ast);

    if (c.global_error) @panic("global_error set"); // TODO ew

    if (true) {
        _ = c.puts(c.TC_CYAN ++ "generated fir:" ++ c.TC_RESET);
        c.Fir_dump(fir);
        _ = c.puts("");
    }

    return true;
}