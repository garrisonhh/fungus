#ifndef CHAR_CLASSIFY_H
#define CHAR_CLASSIFY_H

#include <stdbool.h>

#include "../utils.h"

// from zig vvv
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
// from zig ^^^

#if 0
static inline bool ch_is_alpha(char c) {
    return IN_RANGE(c, 'a', 'z') || IN_RANGE(c, 'A', 'Z');
}

static inline bool ch_is_alphaish(char c) {
    return ch_is_alpha(c) || c == '_';
}

static inline bool ch_is_digit(char c) {
    return IN_RANGE(c, '0', '9');
}

static inline bool ch_is_digitish(char c) {
    return ch_is_digit(c) || c == '_';
}

static inline bool ch_is_word(char c) {
    return ch_is_alphaish(c) || ch_is_digit(c);
}

static inline bool ch_is_space(char c) {
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

static inline bool ch_is_symbol(char c) {
    return IN_RANGE(c, ' ', '~') && !ch_is_word(c) && !ch_is_space(c);
}
#endif

#endif
