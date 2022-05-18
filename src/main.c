#include <stdio.h>
#include <stdbool.h>

#include "fungus.h"
#include "lex.h"
#include "parse.h"
#include "sema.h"
#include "fir.h"

void repl(Names *names) {
    puts(TC_YELLOW "fungus v0 - by garrisonhh" TC_RESET);

    while (!feof(stdin)) {
        // read stdin
        File file = File_read_stdin();
        if (global_error || feof(stdin)) goto cleanup_read;

        // lex
        TokBuf tokbuf = lex(&file, &fungus_lang);

        if (global_error) goto cleanup_lex;

        // parse
        Bump parse_pool = Bump_new();
        AstExpr *ast = parse(&(AstCtx){
            .pool = &parse_pool,
            .file = &file,
            .lang = &fungus_lang
        }, &tokbuf);

        if (global_error) goto cleanup_parse;

        // sema
        sema(&(SemaCtx){
            .pool = &parse_pool,
            .file = &file,
            .lang = &fungus_lang,
            .names = names
        }, ast);

        if (global_error) goto cleanup_parse;

        // fir
        Bump fir_pool = Bump_new();
        const Fir *fir = gen_fir(&fir_pool, &file, ast);

        if (global_error) goto cleanup_fir;

#if 1
        puts(TC_CYAN "generated fir:" TC_RESET);
        Fir_dump(fir);
        puts("");
#endif

        // cleanup
cleanup_fir:
        Bump_del(&fir_pool);
cleanup_parse:
        Bump_del(&parse_pool);
cleanup_lex:
        TokBuf_del(&tokbuf);
cleanup_read:
        File_del(&file);

        global_error = false;
    }
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    types_init();
    names_init();
    Names name_table = Names_new();
    fungus_define_base(&name_table);
    pattern_lang_init(&name_table);
    fungus_lang_init(&name_table);

    DEBUG_SCOPE(0,
        Lang_dump(&fungus_lang);
    );

    repl(&name_table);

    fungus_lang_quit();
    pattern_lang_quit();
    Names_del(&name_table);
    names_quit();
    types_quit();

    return 0;
}
