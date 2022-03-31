#include <stdlib.h>
#include <stdbool.h>

#include "lex.h"
#include "data.h"
#include "utils.h"

#define IN_RANGE(C, A, B) ((C) >= (A) && (C) <= (B))

static bool ch_is_alpha(char c) {
    return IN_RANGE(c, 'a', 'z') || IN_RANGE(c, 'A', 'Z');
}

static bool ch_is_alphaish(char c) {
    return ch_is_alpha(c) || c == '_';
}

static bool ch_is_digit(char c) {
    return IN_RANGE(c, '0', '9');
}

static bool ch_is_digitish(char c) {
    return ch_is_digit(c) || c == '_';
}

static bool ch_is_word(char c) {
    return ch_is_alphaish(c) || ch_is_digit(c);
}

static bool ch_is_space(char c) {
    switch (c) {
    case 0x20:
    case 0x0A:
    case 0x0D:
    case 0x09:
        return true;
    default:
        return false;
    }
}

static bool ch_is_symbol(char c) {
    return IN_RANGE(c, ' ', '~') && !ch_is_word(c) && !ch_is_space(c);
}

static void push_token(TokBuf *tb, TokType type, hsize_t start, hsize_t len) {
    if (tb->len >= tb->cap) {
        tb->cap *= 2;
        tb->types = realloc(tb->types, tb->cap * sizeof(*tb->types));
        tb->starts = realloc(tb->starts, tb->cap * sizeof(*tb->starts));
        tb->lens = realloc(tb->lens, tb->cap * sizeof(*tb->lens));
    }

    tb->types[tb->len] = type;
    tb->starts[tb->len] = start;
    tb->lens[tb->len] = len;
    ++tb->len;
}

static void tokenize(TokBuf *tb, const View *str) {
    hsize_t idx = 0;

    while (true) {
        // skip initial whitespace
        while (ch_is_space(View_get(str, idx)))
            ++idx;

        // see if done
        if (idx == str->len)
            break;

        // parse next token
        hsize_t start = idx;
        TokType type = TOK_INVALID;
        char c = View_get(str, idx);

        if (ch_is_alphaish(c)) {
            // word
            do {
                ++idx;
            } while (ch_is_word(View_get(str, idx)));

            type = TOK_WORD;

            // check if bool
            View word = (View){ &str->str[start], idx - start };

            if (View_eq(&word, &(View){ "true", 4 })
             || View_eq(&word, &(View){ "false", 5 })) {
                type = TOK_BOOL;
            }
        } else if (ch_is_symbol(c)) {
            if (c == '"') {
                // string
                char last;

                do {
                    last = c;
                    c = View_get(str, ++idx);

                    if (!c) {
                        File_error_from(tb->file, start, "unterminated string");
                        goto lex_error;
                    }
                } while (!(c == '"' && last != '\\'));

                ++idx;

                type = TOK_STRING;
            } else {
                // symbol strings
                do {
                    c = View_get(str, ++idx);
                } while (ch_is_symbol(c) && c != '"');

                type = TOK_SYMBOLS;
            }
        } else if (ch_is_digit(c)) {
            // number
            do {
                ++idx;
            } while (ch_is_digitish(View_get(str, idx)));

            if (View_get(str, idx) == '.') {
                do {
                    ++idx;
                } while (ch_is_digitish(View_get(str, idx)));

                type = TOK_FLOAT;
            } else {
                type = TOK_INT;
            }

            if (ch_is_alpha(View_get(str, idx))) {
                // numbers should not have trailing alpha chars
                type = TOK_INVALID;
            } else if (!ch_is_digit(View_get(str, idx - 1))) {
                // numbers should only ever end with digits
                type = TOK_INVALID;
            }
        }

        if (type == TOK_INVALID) {
            File_error_from(tb->file, start, "invalid token");
            goto lex_error;
        }

        push_token(tb, type, start, idx - start);
    }

    return;

lex_error:
    global_error = true;
}

#ifndef TOKBUF_INIT_CAP
#define TOKBUF_INIT_CAP 32
#endif

TokBuf lex(const File *file) {
    // make empty TokBuf
    TokBuf tb = { .cap = TOKBUF_INIT_CAP, .file = file };

    // custom allocator for tokbuf might be dope in the far future
    tb.types = malloc(tb.cap * sizeof(*tb.types));
    tb.starts = malloc(tb.cap * sizeof(*tb.starts));
    tb.lens = malloc(tb.cap * sizeof(*tb.lens));

    // generate tokens
    tokenize(&tb, &file->text);

    return tb;
}

void TokBuf_del(TokBuf *tb) {
    free(tb->types);
    free(tb->starts);
    free(tb->lens);
}

void TokBuf_dump(TokBuf *tb) {
    static const char *colors[TOK_COUNT] = {
        [TOK_SYMBOLS] = TC_WHITE,
        [TOK_WORD] = TC_BLUE,
        [TOK_BOOL] = TC_MAGENTA,
        [TOK_INT] = TC_MAGENTA,
        [TOK_FLOAT] = TC_MAGENTA,
        [TOK_STRING] = TC_GREEN,
    };

    puts(TC_CYAN "Tokens:" TC_RESET);

    size_t col = 0;

    for (size_t i = 0; i < tb->len; ++i) {
        printf("%s", colors[tb->types[i]]);

        if (col + tb->lens[i] > 80) {
            puts("");
            col = 0;
        }

        // TODO this is the only place where tb->str is used
        col += printf("%.*s",
                      (int)tb->lens[i], &tb->file->text.str[tb->starts[i]]);

        printf(TC_RESET " ");
        ++col;
    }

    puts("");
}
