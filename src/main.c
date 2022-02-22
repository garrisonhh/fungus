#include <stdio.h>
#include <stdbool.h>

#include "fungus.h"
#include "lex.h"
#include "syntax.h"

#define REPL_BUF_SIZE 1024

void repl(Fungus *fun) {
    term_format(TERM_YELLOW);
    printf("fungus v0 - by garrisonhh\n");
    term_format(TERM_RESET);

    while (true) {
        char buf[REPL_BUF_SIZE];

        // take input
        printf(">>> ");
        fgets(buf, REPL_BUF_SIZE, stdin);

        if (feof(stdin))
            break;

        // find length
        size_t len;

        for (len = 0; buf[len] && buf[len] != '\n'; ++len)
            ;

        // tokenize
        RawExprBuf rebuf = tokenize(fun, buf, len);

#if 1
        RawExprBuf_dump(fun, &rebuf);
#endif

        if (global_error)
            goto reset_lex;

        // parse
        IterCtx ctx = IterCtx_new();

        /* Expr *ast = */ parse(fun, &ctx, rebuf.exprs, rebuf.len);

        if (global_error)
            goto reset_parse;

reset_parse:
        IterCtx_del(&ctx);
reset_lex:
        RawExprBuf_del(&rebuf);

        global_error = false;
    }
}

int main(void) {
    Fungus fun = Fungus_new();

    repl(&fun);

    Fungus_del(&fun);

    return 0;
}
