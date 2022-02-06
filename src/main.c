#include <stdio.h>
#include <stdbool.h>

#include "lex.h"
#include "syntax.h"

// TODO rewrite this holy shit lmao
static void register_fungus(void) {
    Word as_prec_word = WORD("AddSubPrecedence");
    Word md_prec_word = WORD("MulDivPrecedence");

    PrecId as_prec = prec_unique_id(&as_prec_word);
    PrecId md_prec = prec_unique_id(&md_prec_word);

    Pattern int_pat = { .ty = TYPE(TY_INT) };

#define MATH_RULES\
    X(add, "+", as_prec)\
    X(sub, "-", as_prec)\
    X(mul, "*", md_prec)\
    X(div, "/", md_prec)\
    X(mod, "%", md_prec)\

#define X(NAME, SYM, PREC) {\
    Word sym = WORD(SYM), name = WORD(#NAME);\
    MetaType sym_mty = lex_def_symbol(&sym);\
    MetaType rule_mty = metatype_define(&name);\
    Pattern pat[] = { int_pat, { .mty = sym_mty }, int_pat, };\
    Rule rule = {\
        .pattern = pat,\
        .len = ARRAY_SIZE(pat),\
        .prec = PREC,\
        .ty = TYPE(TY_INT),\
        .mty = rule_mty\
    };\
    syntax_def_rule(&rule);\
}

    MATH_RULES

#undef X
#undef MATH_RULES

    /*
    Word true_kw = WORD("true");
    Word false_kw = WORD("false");

    lex_def_keyword(&true_kw);
    lex_def_keyword(&false_kw);
    */
}

#define REPL_BUF_SIZE 1024

void repl(void) {
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
        RawExprBuf rebuf = tokenize(buf, len);

        if (global_error)
            goto reset_lex;

        // parse
        IterCtx ctx = IterCtx_new();

        /* Expr *ast = */ syntax_parse(&ctx, rebuf.exprs, rebuf.len);

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
    types_init();
    lex_init();
    syntax_init();

    register_fungus();

#if 1
    types_dump();
    syntax_dump();
#endif

    repl();

    syntax_quit();
    lex_quit();
    types_quit();

    return 0;
}
