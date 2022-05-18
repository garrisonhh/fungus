#ifndef SEMA_H
#define SEMA_H

#include "parse.h"
#include "sema/names.h"

/*
 * sema verifies the AST and interprets fungus languages before the AST is
 * turned into the much more static fungus IR (FIR)
 */

typedef struct SemaCtx {
    Bump *pool;
    const File *file;
    const Lang *lang;
    Names *names;
} SemaCtx;

void sema(SemaCtx *, AstExpr *ast);

#endif
