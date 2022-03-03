#include <stdio.h>
#include <stdbool.h>

#include "fungus.h"
#include "lex.h"
#include "syntax.h"

/*
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
        Expr *ast = parse(fun, rebuf.exprs, rebuf.len);

        if (global_error)
            goto reset_parse;

#if 1
        term_format(TERM_CYAN);
        puts("AST:");
        term_format(TERM_RESET);
        Expr_dump(fun, ast);
#endif

reset_parse:
reset_lex:
        RawExprBuf_del(&rebuf);

        global_error = false;
    }
}
*/

static char *read_file(const char *filepath, size_t *out_len) {
    FILE *fp = fopen(filepath, "r");

    if (!fp) {
        fungus_panic("could not open \"%s\"!", filepath);
    }

    fseek(fp, 0, SEEK_END);

    size_t len = ftell(fp);
    char *text = malloc(len + 1);

    text[len] = '\0';
    rewind(fp);
    fread(text, 1, len, fp);

    fclose(fp);

    if (out_len)
        *out_len = len;

    return text;
}

static void compile(Fungus *fun, const char *filename) {
    size_t len;
    const char *text = read_file(filename, &len);

    RawExprBuf rebuf = tokenize(fun, text, len);

#if 1
    RawExprBuf_dump(fun, &rebuf);
#endif

    if (global_error)
        goto reset_lex;

    // parse
    Expr *ast = parse(fun, rebuf.exprs, rebuf.len);

    if (global_error)
        goto reset_parse;

#if 1
    term_format(TERM_CYAN);
    puts("AST:");
    term_format(TERM_RESET);
    Expr_dump(fun, ast);
#endif

reset_parse:
reset_lex:
    RawExprBuf_del(&rebuf);
}

int main(int argc, char **argv) {
    Fungus fun = Fungus_new();

    // repl(&fun);

    if (argc != 2)
        fungus_panic("expecting one argument, a filename");

    compile(&fun, argv[1]);

    Fungus_del(&fun);

    return global_error;
}
