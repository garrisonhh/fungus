#include <stdio.h>
#include <stdbool.h>

#include "file.h"
#include "lex.h"

void repl(void) {
    puts(TC_YELLOW "fungus v0 - by garrisonhh" TC_RESET);

    while (!feof(stdin)) {
        // read stdin
        File file = File_read_stdin();
        if (global_error) goto cleanup_read;

        // lex
        TokBuf tokbuf = lex(&file);
        if (global_error) goto cleanup_lex;

#if 1
        TokBuf_dump(&tokbuf);
#endif

        // cleanup
cleanup_lex:
        TokBuf_del(&tokbuf);
cleanup_read:
        File_del(&file);

        global_error = false;
    }
}


#include "lang/pattern.h"
#include "lang/rules.h"

int main(int argc, char **argv) {
    // TODO opt parsing eventually

#if 1
    RuleTree rt = RuleTree_new();

    Rule_define(&rt, &(RuleDef){
        .name = WORD("Add"),
        .pat = Pattern_from(&rt.pool, "a: Number `+ b: Number")
    });
    Rule_define(&rt, &(RuleDef){
        .name = WORD("Subtract"),
        .pat = Pattern_from(&rt.pool, "a: Number `- b: Number")
    });
    Rule_define(&rt, &(RuleDef){
        .name = WORD("Multiply"),
        .pat = Pattern_from(&rt.pool, "a: Number `* b: Number")
    });
    Rule_define(&rt, &(RuleDef){
        .name = WORD("Divide"),
        .pat = Pattern_from(&rt.pool, "a: Number `/ b: Number")
    });
    Rule_define(&rt, &(RuleDef){
        .name = WORD("Modulo"),
        .pat = Pattern_from(&rt.pool, "a: Number `% b: Number")
    });

    RuleTree_dump(&rt);

    RuleTree_del(&rt);
    exit(0);
#endif

    repl();

    return 0;
}
