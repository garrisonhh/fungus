#include <stdio.h>
#include <stdbool.h>

#include "lex.h"
#include "syntax.h"

static void define_math_rule(RuleTree *rt, const char *sym, Prec prec,
                             RuleHook hook) {
    const Type num_types[] = { TYPE(TY_INT), TYPE(TY_FLOAT) };

    Word word = WORD(sym);
    MetaType mty = metatype_define(&word);

    for (size_t i = 0; i < ARRAY_SIZE(num_types); ++i) {
        Type ty = num_types[i];

        RuleAtom pat[] = {
            { .ty = ty },
            { .mty = mty },
            { .ty = ty },
        };

        RuleDef def = {
            .pattern = pat,
            .len = ARRAY_SIZE(pat),
            .prec = prec,
            .ty = ty,
            .mty = mty,
            .interpret = hook
        };

        RuleTree_define_rule(rt, &def);
    }
}

static void register_fungus(void) {
    RuleTree test = RuleTree_new();

    // precedences
    PrecDef lowest_def = {
        .name = WORD("LowestPrecedence")
    };
    Prec lowest = Prec_define(&test.prec_graph, &lowest_def);

    Prec addsub_above[] = { lowest };
    PrecDef addsub_def = {
        .name = WORD("AddSubPrecedence"),
        .above = addsub_above,
        .above_len = ARRAY_SIZE(addsub_above),
    };
    Prec addsub = Prec_define(&test.prec_graph, &addsub_def);

    Prec muldiv_above[] = { addsub };
    PrecDef muldiv_def = {
        .name = WORD("MulDivPrecedence"),
        .above = muldiv_above,
        .above_len = ARRAY_SIZE(muldiv_above),
    };
    Prec muldiv = Prec_define(&test.prec_graph, &muldiv_def);

    Prec highest_above[] = { addsub, muldiv };
    PrecDef highest_def = {
        .name = WORD("HighestPrecedence"),
        .above = highest_above,
        .above_len = ARRAY_SIZE(highest_above),
    };
    Prec highest = Prec_define(&test.prec_graph, &highest_def);

    // basic math operators
    define_math_rule(&test, "+", addsub, NULL);
    define_math_rule(&test, "-", addsub, NULL);
    define_math_rule(&test, "*", muldiv, NULL);
    define_math_rule(&test, "/", muldiv, NULL);

    // blocks
    Word lparen_sym = WORD("(");
    Word rparen_sym = WORD(")");
    Word parens_name = WORD("parens");

    MetaType lparen_mty = metatype_define(&lparen_sym);
    MetaType rparen_mty = metatype_define(&rparen_sym);
    MetaType parens_mty = metatype_define(&parens_name);

    RuleAtom parens_pat[] = {
        { .mty = lparen_mty },
        { .modifiers = RAM_REPEAT },
        { .mty = rparen_mty },
    };

    RuleDef parens_def = {
        .pattern = parens_pat,
        .len = ARRAY_SIZE(parens_pat),
        .prec = highest,
        .ty = TYPE(TY_NONE),
        .mty = parens_mty
    };

    RuleTree_define_rule(&test, &parens_def);

#if 1
    RuleTree_dump(&test);
    exit(0);
#endif
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
#endif

    repl();

    syntax_quit();
    lex_quit();
    types_quit();

    return 0;
}
