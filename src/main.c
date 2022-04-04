#include <stdio.h>
#include <stdbool.h>

#include "file.h"
#include "lex.h"
#include "parse.h"
#include "lang/fungus.h"

void repl(void) {
    Lang fun = base_fungus();

    Lang_dump(&fun);

    puts(TC_YELLOW "fungus v0 - by garrisonhh" TC_RESET);

    while (!feof(stdin)) {
        // read stdin
        File file = File_read_stdin();
        if (global_error || feof(stdin)) goto cleanup_read;

        // lex
        TokBuf tokbuf = lex(&file);
        if (global_error) goto cleanup_lex;

#if 0
        TokBuf_dump(&tokbuf);
        puts("");
#endif

        // parse
        Bump parse_pool = Bump_new();
        Expr *ast = parse(&parse_pool, &fun, &tokbuf);
        if (global_error) goto cleanup_parse;

#if 1
        puts(TC_CYAN "AST:" TC_RESET);
        Expr_dump(ast, tokbuf.file);
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

    Lang_del(&fun);
}

int main(int argc, char **argv) {
    // TODO opt parsing eventually

    repl();

    return 0;
}
