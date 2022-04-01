#include <string.h>
#include <ctype.h>

#include "pattern.h"
#include "../lex.h"

static Word word_of_tok(TokBuf *tokens, size_t idx) {
    return Word_new(&tokens->file->text.str[tokens->starts[idx]],
                    tokens->lens[idx]);
}

// assumes pattern is correct since this is not user-facing
Pattern Pattern_from(Bump *pool, const char *str) {
    File f = File_from_str("pattern", str, strlen(str));
    TokBuf tokens = lex(&f);

    // copy exprs + lexemes
    Vec words = Vec_new(), is_expr = Vec_new();

    for (size_t i = 1; i < tokens.len; ++i) {
        if (tokens.types[i] == TOK_SYMBOLS) {
            Word tok = word_of_tok(&tokens, i);

            if (Word_eq_view(&tok, &(View){ ":", 1 })) {
                Word expr_ident = word_of_tok(&tokens, i - 1);

                Vec_push(&words, Word_copy_of(&expr_ident, pool));
                Vec_push(&is_expr, (void *)1);
            } else if (tok.str[0] == '`') {
                Word lxm = Word_new(&tok.str[1], tok.len - 1);

                Vec_push(&words, Word_copy_of(&lxm, pool));
                Vec_push(&is_expr, NULL);
            }
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

void Pattern_print(Pattern *pat) {
    for (size_t i = 0; i < pat->len; ++i) {
        if (i) printf(" ");

        const Word *word = pat->pat[i];

        printf("%s%.*s" TC_RESET,
               pat->is_expr[i] ? TC_BLUE : TC_WHITE,
               (int)word->len, word->str);
    }
}
