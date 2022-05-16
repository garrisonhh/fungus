#ifndef LEX_H
#define LEX_H

#include "file.h"

typedef struct Lang Lang;

typedef enum TokType {
    TOK_INVALID,
    TOK_LEXEME,
    TOK_IDENT,
    TOK_BOOL,
    TOK_INT,
    TOK_FLOAT,
    TOK_STRING,

    // fungus-specific tokenization stuff:
    // TODO integrate with parser once implemented
    TOK_ESCAPE, // '`' for escaping a symbol or word
    TOK_SCOPE, // from lcurly -> rcurly

    TOK_COUNT
} TokType;

typedef struct TokBuf {
    void *zig_tbuf;

    TokType *types;
    hsize_t *starts, *lens;
    size_t len;
} TokBuf;

TokBuf lex(const File *, const Lang *);
void TokBuf_del(TokBuf *);

void TokBuf_dump(TokBuf *, const File *);

#endif
