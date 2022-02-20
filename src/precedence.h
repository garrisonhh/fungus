#ifndef PRECEDENCE_H
#define PRECEDENCE_H

/*
 * precedence is represented by a directed acyclic graph, allowing for intuitive
 * and symbolic runtime definition of precedences.
 */

#include "utils.h"
#include "words.h"

typedef struct PrecHandle { unsigned id; } Prec;

#define MAX_PRECEDENCES 256

typedef struct PrecEntry {
    Word *name;

    // list of precedences this precedence is higher than
    // TODO eventually a smarter data structure, like a hash set?
    Prec above[MAX_PRECEDENCES];
    size_t above_len;
} PrecEntry;

typedef struct PrecGraph {
    Bump pool;
    Vec entries; // entries indexed by id

    // TODO name => id hashmap
} PrecGraph;

typedef struct PrecDef {
    Word name;
    Prec *above, *below;
    size_t above_len, below_len;
} PrecDef;

PrecGraph PrecGraph_new(void);
void PrecGraph_del(PrecGraph *);

Prec Prec_define(PrecGraph *, PrecDef *def);
Comparison Prec_cmp(PrecGraph *, Prec a, Prec b);

void PrecGraph_dump(PrecGraph *);

#endif
