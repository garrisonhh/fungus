#ifndef LEX_H
#define LEX_H

#include "file.h"

/*
 * TODO file awareness for nice errors
 */

#define TOK_TYPES\
    TYPE(INVALID)\
    TYPE(WORD)\
    TYPE(SYMBOLS)\
    TYPE(BOOL)\
    TYPE(INT)\
    TYPE(FLOAT)\
    TYPE(STRING)

#define TYPE(NAME) TOK_##NAME,
typedef enum TokType { TOK_TYPES TOK_COUNT } TokType;
#undef TYPE

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
