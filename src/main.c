#include <stdio.h>
#include <stdbool.h>

#include "lex.h"
#include "syntax.h"

static void define_math_rule(RuleTree *rt, const char *sym, PrecId prec,
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
    // precedences
    Word as_prec_word = WORD("AddSubPrecedence");
    Word md_prec_word = WORD("MulDivPrecedence");
    Word highest_prec_word = WORD("HighestPrecedence");

    PrecId as_prec = prec_unique_id(&as_prec_word);
    PrecId md_prec = prec_unique_id(&md_prec_word);
    PrecId highest_prec = prec_unique_id(&highest_prec_word);

    // rules
    RuleTree test = RuleTree_new();

    // basic math operators
    define_math_rule(&test, "+", as_prec, NULL);
    define_math_rule(&test, "-", as_prec, NULL);
    define_math_rule(&test, "*", md_prec, NULL);
    define_math_rule(&test, "/", md_prec, NULL);

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
        .prec = highest_prec,
        // TODO this isn't right, maybe a type placeholder? like TY_MUST_INFER
        // so that the compiler knows the type is unknown while making the tree?
        // maybe another function hook? idk
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
