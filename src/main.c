#include <stdio.h>
#include <stdbool.h>

#include "file.h"
#include "lex.h"

#define REPL_BUF_SIZE 1024

void repl(void) {
    puts(TC_YELLOW "fungus v0 - by garrisonhh" TC_RESET);

    while (true) {
        // read stdin
        File file = File_read_stdin();
        if (global_error) goto cleanup_read;

        // lex
        TokBuf tokbuf = lex(&file);
        if (global_error) goto cleanup_lex;

#if 1
        TokBuf_dump(&tokbuf);
#endif

cleanup_lex:
        TokBuf_del(&tokbuf);
cleanup_read:
        File_del(&file);

        global_error = false;
    }
}

int main(int argc, char **argv) {
    // TODO opt parsing eventually

    repl();

    return 0;
}
