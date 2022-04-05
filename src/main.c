#include <stdio.h>
#include <stdbool.h>

#include "file.h"
#include "lex.h"
#include "parse.h"
#include "lang/fungus.h"
#include "lang/pattern.h"

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
        Bump parse_pool = Bump_new();
        RExpr *ast = parse(&parse_pool, &fungus_lang, &tokbuf);
        if (global_error) goto cleanup_parse;

#if 1
        puts(TC_CYAN "AST:" TC_RESET);
        RExpr_dump(ast, &fungus_lang, tokbuf.file);
#endif

        // cleanup
cleanup_parse:
        Bump_del(&parse_pool);
cleanup_lex:
        TokBuf_del(&tokbuf);
cleanup_read:
        File_del(&file);

        global_error = false;
    }
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    fungus_lang_init();
    pattern_lang_init();

    // TODO opt parsing eventually

    repl();

    pattern_lang_quit();
    fungus_lang_quit();

    return 0;
}
