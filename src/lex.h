#ifndef LEX_H
#define LEX_H

#include <stddef.h>
#include <stdint.h>

/*
 * TODO file awareness for nice errors
 */

typedef uint32_t hsize_t;

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

    // TODO File construct here
    const char *str;
} TokBuf;

TokBuf lex(const char *str, size_t len);
void TokBuf_del(TokBuf *);

void TokBuf_dump(TokBuf *);

#endif
