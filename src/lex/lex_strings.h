#ifndef LEX_STRINGS_H
#define LEX_STRINGS_H

#include <stddef.h>

// just forwarding functions from lex_strings.zig

typedef enum CharClass {
    CH_EOF,
    CH_SPACE,
    CH_ALPHA,
    CH_DIGIT,
    CH_UNDERSCORE,
    CH_SYMBOL
} CharClass;

CharClass classify_char(char ch);
const char *CharClass_name(CharClass);

// returns malloc'd string
char *escape_cstr(const char *str, size_t len);

#endif
