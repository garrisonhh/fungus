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

        // tokenize
        TokBuf tokbuf = lex(&file);

#if 1
        TokBuf_dump(&tokbuf);
#endif

        TokBuf_del(&tokbuf);
        File_del(&file);
    }
}

int main(int argc, char **argv) {
    // TODO opt parsing eventually

    repl();

    return 0;
}
