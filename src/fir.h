#ifndef FIR_H
#define FIR_H

#include <assert.h>

#include "lang/ast_expr.h"

/*
 * Fungus IR, a very static AST representation more like a traditional language
 * AST. this allows much easier lowering to an SSA IR.
 */

typedef struct Fir Fir; // opaque struct ptr (implemented in zig)

const Fir *gen_fir(Bump *, const File *, const AstExpr *ast);

void Fir_dump(const Fir *);

#endif
