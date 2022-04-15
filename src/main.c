#include <stdio.h>
#include <stdbool.h>

#include "file.h"
#include "lex.h"
#include "parse.h"
#include "lang/fungus.h"
#include "lang/pattern.h"

#if 0
void repl(void) {
    Lang_dump(&fungus_lang);

    puts(TC_YELLOW "fungus v0 - by garrisonhh" TC_RESET);

    while (!feof(stdin)) {
        // read stdin
        File file = File_read_stdin();
        if (global_error || feof(stdin)) goto cleanup_read;

        // lex
        TokBuf tokbuf = lex(&file);
        if (global_error) goto cleanup_lex;

        // parse
#if 0
        Bump parse_pool = Bump_new();
        RExpr *raw_ast = parse(&parse_pool, &fungus_lang, &tokbuf);
        if (global_error) goto cleanup_parse;

#if 1
        puts(TC_CYAN "AST:" TC_RESET);
        RExpr_dump(raw_ast, &fungus_lang, tokbuf.file);
#endif


        // cleanup
cleanup_parse:
        Bump_del(&parse_pool);
#endif
cleanup_lex:
        TokBuf_del(&tokbuf);
cleanup_read:
        File_del(&file);

        global_error = false;
    }
}
#endif

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

#if 1
    Bump pool = Bump_new();

    pattern_lang_init();

    precompile_pattern(&pool,
        "a: Literal | Rule ! T `+ b: Literal | Rule ! T -> T\n"
        "where T is Number\n");
    precompile_pattern(&pool,
        "`++ expr: Ident ! T -> T where T is Number\n");

    pattern_lang_quit();

    Bump_del(&pool);
#else
    pattern_lang_init();
    fungus_lang_init();

    // TODO opt parsing eventually

    repl();

    fungus_lang_quit();
    pattern_lang_quit();
#endif

    return 0;
}
