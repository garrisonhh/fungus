#ifndef PATTERN_H
#define PATTERN_H

#include "../data.h"

/*
 * TODO:
 * - match should be able to differentiate lexemes, scopes, rules, and other
 *   (this is a `parse` problem)
 * - type checking should then be able to check scopes, rules, and other for
 *   runtime type information
 *   (this is a `sema` problem)
 */

typedef enum MatchType {
    MATCH_EXPR,
    MATCH_LEXEME,
    MATCH_SCOPE,
    MATCH_RULE
} MatchType;

typedef struct Where {
    View name;
    View is;
} Where;

typedef struct Pattern2 {
    // parallel arrays
    Word *atoms;
    MatchType *matches;
    size_t len;

    // types of params + return, stored for sema to compile and check
    View *types;
    View return_type;

    // templating stuff
    Where *wheres;
    size_t where_len;
} Pattern2;

// create using Pattern_from
typedef struct Pattern {
    // parallel arrays:
    // if is_expr[i], then match any expr. else, match lexeme pat[i]
    const Word **pat;
    bool *is_expr; // TODO use MatchType instead
    size_t len;

    // stored for sema to compile and check
    View *types;
    View return_type;
} Pattern;

void pattern_lang_init(void);
void pattern_lang_quit(void);

/*
 * Patterns are compiled from a bastardized syntax for internal use, this is
 * simply because writing out large struct definitions for patterns is annoying,
 * especially when the compiler can do it for me.
 *
 * this small DSL simply looks something like this:
 * "a: T `+ b: T -> T where T = Number"
 *
 * (more formally:)
 * - a list of `ident: typeexpr`s or lexemes escaped with a `
 *
 * in the future I can make my own pattern dsl from within fungus itself!
 */
Pattern Pattern_from(Bump *, const char *str);

void Pattern_print(const Pattern *);

#endif
