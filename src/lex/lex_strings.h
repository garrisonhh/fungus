#ifndef LEX_STRINGS_H
#define LEX_STRINGS_H

#include <stdbool.h>

#include "../utils.h"

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

#endif
