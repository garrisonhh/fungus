#include <ctype.h>

#include "lang.h"
#include "fungus.h"
#include "lang/ast_expr.h"
#include "lex/char_classify.h"

Lang Lang_new(Word name) {
    // copy name
    char *copy = malloc(name.len * sizeof(*copy));

    for (size_t i = 0; i < name.len; ++i)
        copy[i] = name.str[i];

    return (Lang){
        .name = { .str = copy, .len = name.len, .hash = name.hash },
        .rules = RuleTree_new(),
        .precs = Precs_new(),
        .words = HashSet_new(),
        .syms = HashSet_new()
    };
}

void Lang_del(Lang *lang) {
    HashSet_del(&lang->syms);
    HashSet_del(&lang->words);
    Precs_del(&lang->precs);
    RuleTree_del(&lang->rules);

    free((char *)lang->name.str);
}

Rule Lang_legislate(Lang *lang, const File *file, Type type,
                    Prec prec, AstExpr *pat_ast) {
    return Rule_define(&lang->rules, file, type, prec, pat_ast);
}

Rule Lang_immediate_legislate(Lang *lang, Type type, Prec prec, Pattern pat) {
    Rule rule = Rule_immediate_define(&lang->rules, type, prec, pat);

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

void Lang_crystallize(Lang *lang, Names *names) {
    RuleTree_crystallize(&lang->rules, names);

    // extract lexemes
    for (size_t i = 0; i < lang->rules.entries.len; ++i) {
        if (i == lang->rules.rule_scope.id)
            continue;

        const RuleEntry *entry = lang->rules.entries.data[i];
        const MatchAtom *matches = entry->pat.matches;

        for (size_t j = 0; j < entry->pat.len; ++j) {
            const MatchAtom *match = &matches[j];

            if (match->type == MATCH_LEXEME) {
                if (ch_is_symbol(match->lxm->str[0]))
                    HashSet_put(&lang->syms, match->lxm);
                else
                    HashSet_put(&lang->words, match->lxm);
            }
        }
    }
}

Prec Lang_make_prec(Lang *lang, Word name, Associativity assoc) {
    return Prec_define(&lang->precs, name, assoc);
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

    Precs_dump(&lang->precs);
    RuleTree_dump(&lang->rules);
}
