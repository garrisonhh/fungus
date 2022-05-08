#ifndef LEX_H
#define LEX_H

#include "file.h"

typedef enum TokType {
    TOK_INVALID,
    TOK_WORD,
    TOK_SYMBOLS,
    TOK_BOOL,
    TOK_INT,
    TOK_FLOAT,
    TOK_STRING
} TokType;

typedef struct TokBuf {
    TokType *types;
    hsize_t *starts, *lens;
    size_t len, cap;

    const File *file;
} TokBuf;

TokBuf lex(const File *);
void TokBuf_del(TokBuf *);

void TokBuf_dump(TokBuf *);

#endif
