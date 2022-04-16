#ifndef SEMA_H
#define SEMA_H

#include "parse.h"
#include "sema/names.h"

typedef struct SemaCtx {
    Bump *pool;
    const Lang *lang;
    const Names *names;
} SemaCtx;

void sema(SemaCtx *, AstExpr *ast);

#endif
