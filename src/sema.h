#ifndef SEMA_H
#define SEMA_H

#include "parse.h"

Expr *sema(Bump *, const Lang *, Expr *);

#endif
