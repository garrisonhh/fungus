#ifndef SEMA_H
#define SEMA_H

#include "parse.h"
#include "sema/names.h"

/*
 * sema type infers + checks the AST and applies interpretation of the dynamic
 * compile-time layer of fungus into the static layer (like fungus languages)
 */

typedef struct SemaCtx {
    Bump *pool;
    const File *file;
    const Lang *lang;
    Names *names;
} SemaCtx;

void sema(SemaCtx *, AstExpr *ast);

#endif
