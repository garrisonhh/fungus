#include <assert.h>

#include "pattern.h"
#include "../lang.h"
#include "../lex.h"

Lang pattern_lang;

#define PRECS\
    PREC("Lowest",  LEFT)\
    PREC("Decl",    LEFT)\
    PREC("Bang",    LEFT)\
    PREC("Or",      LEFT)\
    PREC("Highest", LEFT)

#define RULES\
    RULE("MatchExpr", "Decl", 3, {{0}, { ":", MATCH_LEXEME }, {0}})\
    RULE("TypeOr",    "Or",   3, {{0}, { "|", MATCH_LEXEME }, {0}})\
    RULE("TypeBang",  "Bang", 3, {{0}, { "!", MATCH_LEXEME }, {0}})\

void pattern_lang_init(void) {
    Lang lang = Lang_new(WORD("PatternLang"));

    // precedences
    {
#define PREC(NAME, ASSOC) WORD(NAME),
        Word prec_names[] = { PRECS };
#undef PREC
#define PREC(NAME, ASSOC) ASSOC_##ASSOC,
        Associativity prec_assocs[] = { PRECS };
#undef PREC

        Prec last;

        for (size_t i = 0; i < ARRAY_SIZE(prec_names); ++i) {
            size_t above_len = 0;
            Prec *above = NULL;

            if (i) {
                above_len = 1;
                above = &last;
            }

            last = Lang_make_prec(&lang, &(PrecDef){
                .name = prec_names[i],
                .assoc = prec_assocs[i],
                .above = above,
                .above_len = above_len
            });
        }
    }

    // rules
    typedef struct RuleAtom {
        const char *word;
        MatchType matches;
    } RuleAtom;

    {
#define RULE(NAME, PREC, PAT_LEN, ...) WORD(NAME),
        Word names[] = { RULES };
#undef RULE
#define RULE_COUNT (ARRAY_SIZE(names))

        Prec precs[RULE_COUNT];
        {
#define RULE(NAME, PREC, PAT_LEN, ...) WORD(PREC),
            Word prec_names[] = { RULES };
#undef RULE

            for (size_t i = 0; i < RULE_COUNT; ++i)
                precs[i] = Prec_by_name(&lang.precs, &prec_names[i]);
        }

#define RULE(NAME, PREC, PAT_LEN, ...) PAT_LEN,
        size_t lens[] = { RULES };
#undef RULE
#define RULE(NAME, PREC, PAT_LEN, ...) __VA_ARGS__,
        RuleAtom atoms[][32] = { RULES };
#undef RULE

        Bump *pool = &lang.rules.pool;

        for (size_t i = 0; i < RULE_COUNT; ++i) {
            // create pattern from metaprogrammed stuff
            RuleAtom *pattern = atoms[i];
            size_t len = lens[i];

            // pattern lang is unique in that it is only used to generate an
            // AST, so typing etc. can be ignored
            Pattern2 pat = {
                .atoms = Bump_alloc(pool, len * sizeof(*pat.atoms)),
                .matches = Bump_alloc(pool, len * sizeof(*pat.matches)),
                .len = len
            };

            for (size_t j = 0; j < len; ++j) {
                if (pattern->word)
                    pat.atoms[j] = WORD(pattern->word);
                pat.matches[j] = pattern->matches;
            }

            continue;

            // legislate
            Lang_legislate(&lang, &(RuleDef){
                .name = names[i],
                .prec = precs[i],
                // TODO .pat = pat
            });
        }

#undef RULE_COUNT
    }

    pattern_lang = lang;

#ifdef DEBUG
    Lang_dump(&pattern_lang);
#endif
}

void pattern_lang_quit(void) {
    Lang_del(&pattern_lang);
}

static Word word_of_tok(TokBuf *tokens, size_t idx) {
    const char *text = tokens->file->text.str;

    return Word_new(&text[tokens->starts[idx]], tokens->lens[idx]);
}

// assumes pattern is correct since this is not user-facing
// lol this is a mess and will be replaced.
Pattern Pattern_from(Bump *pool, const char *str) {
    File f = File_from_str("pattern", str, strlen(str));
    TokBuf tokens = lex(&f);

    // copy exprs + lexemes
    Vec words = Vec_new(), is_expr = Vec_new();
    bool capture_lxm = false, capture_return = false;

    for (size_t i = 0; i < tokens.len; ++i) {
        if (tokens.types[i] == TOK_SYMBOLS) {
            Word tok = word_of_tok(&tokens, i);

            if (Word_eq_view(&tok, &(View){ "->", 2 })) {
                assert(i <= tokens.len - 2);
                capture_return = true;
            } else if (i > 0 && Word_eq_view(&tok, &(View){ ":", 1 })) {
                Word expr_ident = word_of_tok(&tokens, i - 1);

                Vec_push(&words, Word_copy_of(&expr_ident, pool));
                Vec_push(&is_expr, (void *)1);
            } else if (tok.str[0] == '`') {
                if (tok.len > 1) {
                    Word lxm = Word_new(&tok.str[1], tok.len - 1);

                    Vec_push(&words, Word_copy_of(&lxm, pool));
                    Vec_push(&is_expr, NULL);
                } else {
                    capture_lxm = true;
                }
            }
        } else if (tokens.types[i] == TOK_WORD && capture_lxm) {
            Word lxm = word_of_tok(&tokens, i);

            Vec_push(&words, Word_copy_of(&lxm, pool));
            Vec_push(&is_expr, NULL);

            capture_lxm = false;
        } else if (tokens.types[i] == TOK_WORD && capture_return) {
            // TODO

            capture_return = true;
        }
    }

    // move words to pattern
    Pattern pat = {
        .pat = Bump_alloc(pool, words.len * sizeof(const Word *)),
        .is_expr = Bump_alloc(pool, words.len * sizeof(bool)),
        .len = words.len
    };

    for (size_t i = 0; i < pat.len; ++i) {
        pat.pat[i] = Word_copy_of(words.data[i], pool);
        pat.is_expr[i] = (bool)is_expr.data[i];
    }

    Vec_del(&is_expr);
    Vec_del(&words);
    TokBuf_del(&tokens);
    File_del(&f);

    return pat;
}

void Pattern_print(const Pattern *pat) {
    for (size_t i = 0; i < pat->len; ++i) {
        if (i) printf(" ");

        const Word *word = pat->pat[i];

        printf("%s%.*s" TC_RESET,
               pat->is_expr[i] ? TC_BLUE : TC_WHITE,
               (int)word->len, word->str);
    }
}
