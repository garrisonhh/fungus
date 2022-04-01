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

#if 1
#include "lang.h"
#include "lang/fungus.h"
#endif

int main(int argc, char **argv) {
    // TODO opt parsing eventually

#if 1
    Lang fun = base_fungus();

    Lang_dump(&fun);

    Lang_del(&fun);
    exit(0);
#endif

    repl();

    return 0;
}
