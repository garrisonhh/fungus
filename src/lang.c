#include <ctype.h>

#include "lang.h"

Lang Lang_new(Word name) {
    // copy name
    char *copy = malloc(name.len * sizeof(*copy));

    for (size_t i = 0; i < name.len; ++i)
        copy[i] = name.str[i];

    return (Lang){
        .name = { .str = copy, .len = name.len, .hash = name.hash },
        .rules = RuleTree_new(),
        .precs = PrecGraph_new(),
        .words = HashSet_new(),
        .syms = HashSet_new()
    };
}

void Lang_del(Lang *lang) {
    HashSet_del(&lang->syms);
    HashSet_del(&lang->words);
    PrecGraph_del(&lang->precs);
    RuleTree_del(&lang->rules);

    free((char *)lang->name.str);
}

// searches for lexeme literals recursively and defines any
static void define_ast_lexemes(Lang *lang, AstExpr *expr) {
    UNIMPLEMENTED;
}

Rule Lang_legislate(Lang *lang, Word word, Prec prec, AstExpr *pat_ast) {
    Rule rule = Rule_define(&lang->rules, word, prec, pat_ast);

    define_ast_lexemes(lang, pat_ast);

    return rule;
}

Rule Lang_immediate_legislate(Lang *lang, Word word, Prec prec, Pattern pat) {
    Rule rule = Rule_immediate_define(&lang->rules, word, prec, pat);

    // define symbols + words in hashsets
    for (size_t i = 0; i < pat.len; ++i) {
        MatchAtom *atom = &pat.matches[i];

        if (atom->type == MATCH_LEXEME) {
            HashSet *set =
                ispunct(atom->lxm->str[0]) ? &lang->syms : &lang->words;

            HashSet_put(set, atom->lxm);
        }
    }

    return rule;
}

Prec Lang_make_prec(Lang *lang, PrecDef *prec_def) {
    Prec prec = Prec_define(&lang->precs, prec_def);

    // will need to do something here at some point I'm sure

    return prec;
}

void Lang_dump(const Lang *lang) {
    printf(TC_YELLOW "language %.*s:\n" TC_RESET,
           (int)lang->name.len, lang->name.str);

    printf(TC_CYAN "words: " TC_RESET);
    HashSet_print(&lang->words);
    puts("");
    printf(TC_CYAN "syms: " TC_RESET);
    HashSet_print(&lang->syms);
    puts("\n");

    PrecGraph_dump(&lang->precs);
    RuleTree_dump(&lang->rules);
}
