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

#endif
