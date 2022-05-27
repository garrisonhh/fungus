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

typedef struct Token {
    TokType type;
    hsize_t start, len;
} Token;

typedef struct TokBuf TokBuf; // opaque ptr; impl'd in zig

// TODO direly needs an allocator context to avoid `malloc` calls
TokBuf *TokBuf_new(void);
void TokBuf_del(TokBuf *);

size_t TokBuf_len(const TokBuf *);
Token TokBuf_get(const TokBuf *, size_t index);

// adds tokens to tokbuf, returns success
bool lex(TokBuf *, const File *, const Lang *, size_t start, size_t len);

void TokBuf_dump(const TokBuf *, const File *);

#endif
