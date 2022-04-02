#ifndef PATTERN_H
#define PATTERN_H

#include "../data.h"

// create using Pattern_from
// TODO storing typeexprs + typechecking patterns during sema?
typedef struct Pattern {
    // parallel arrays:
    // if is_expr[i], then match any expr. else, match lexeme pat[i]
    const Word **pat;
    bool *is_expr;
    size_t len;
} Pattern;

/*
 * Patterns are compiled from a bastardized syntax for internal use, this is
 * simply because writing out large struct definitions for patterns is annoying,
 * especially when the compiler can do it for me.
 *
 * this small DSL simply looks something like this:
 * "a: Number `+ b: Number"
 *
 * (more formally:)
 * - a list of `ident: typeexpr`s or lexemes escaped with a `
 *
 * in the future I can make my own pattern dsl from within fungus itself!
 */
Pattern Pattern_from(Bump *, const char *str);

void Pattern_print(const Pattern *);

#endif
