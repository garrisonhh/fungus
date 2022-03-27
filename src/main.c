#include <stdio.h>
#include <stdbool.h>

#include "fungus.h"
#include "lex.h"
#include "parse.h"
#include "ir.h"
#include "cgen.h"

static bool eval(const char *text, size_t len) {
    // tokenize
    // TODO
}

#define REPL_BUF_SIZE 1024

void repl(void) {
    puts(TC_YELLOW "fungus v0 - by garrisonhh" TC_RESET);

    while (true) {
        char buf[REPL_BUF_SIZE];

        // take input
        printf(">>> ");
        fgets(buf, REPL_BUF_SIZE, stdin);

        if (feof(stdin))
            break;

        // find length
        size_t len;

        for (len = 0; buf[len] && buf[len] != '\n'; ++len)
            ;

        // tokenize
        TokBuf tokbuf = lex(buf, len);

#if 1
        TokBuf_dump(&tokbuf);
#endif

        TokBuf_del(&tokbuf);
    }
}

/*
static char *read_file(const char *filepath, size_t *out_len) {
    FILE *fp = fopen(filepath, "r");

    if (!fp) {
        fungus_panic("could not open \"%s\"!", filepath);
    }

    fseek(fp, 0, SEEK_END);

    size_t len = ftell(fp);
    char *text = malloc(len + 1);

    text[len] = '\0';
    rewind(fp);
    fread(text, 1, len, fp);

    fclose(fp);

    if (out_len)
        *out_len = len;

    return text;
}
*/

int main(int argc, char **argv) {
    ir_init();

    // TODO opt parsing eventually
    repl();

    return 0;
}
