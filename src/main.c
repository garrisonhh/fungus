#include <stdio.h>
#include <stdbool.h>

#include "fungus.h"
#include "lex.h"
#include "parse.h"
#include "sema.h"
#include "fir.h"

// returns success
bool try_compile_file(File *file, Names *names) {
    // lex
    TokBuf tokbuf = TokBuf_new();

    lex(&tokbuf, file, &fungus_lang, 0, file->text.len);

    if (global_error) goto cleanup_lex;

    // parse
    Bump parse_pool = Bump_new();
    AstExpr *ast = parse(&(AstCtx){
        .pool = &parse_pool,
        .file = file,
        .lang = &fungus_lang
    }, &tokbuf);

    if (global_error) goto cleanup_parse;

    // sema
    sema(&(SemaCtx){
        .pool = &parse_pool,
        .file = file,
        .lang = &fungus_lang,
        .names = names
    }, ast);

    if (global_error) goto cleanup_parse;

#if 1
    puts(TC_CYAN "generated ast:" TC_RESET);
    AstExpr_dump(ast, &fungus_lang, file);
    puts("");
#endif

    // fir
    Bump fir_pool = Bump_new();
    const Fir *fir = gen_fir(&fir_pool, file, ast);

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

    bool success = !global_error;
    global_error = false;

    return success;
}

void repl(Names *names) {
    while (!feof(stdin)) {
        File file = File_read_stdin();

        if (global_error || feof(stdin)) {
            File_del(&file);
            global_error = false;
        } else if (!try_compile_file(&file, names)) {
            return;
        }
    }
}

// returns success
bool test_file(char *filepath, Names *names) {
    File file = File_open(filepath);

    printf(TC_GREEN "testing '%s':" TC_RESET "\n", filepath);

    puts("```");
    printf("%s", file.text.str);
    puts("\n```");

    if (!try_compile_file(&file, names))
        return false;

    File_del(&file);

    return true;
}

int main(int argc, char **argv) {
    puts(TC_YELLOW "fungus v0 - by garrisonhh" TC_RESET);

    types_init();
    names_init();
    Names name_table = Names_new();
    fungus_define_base(&name_table);
    pattern_lang_init(&name_table);
    fungus_lang_init(&name_table);

    DEBUG_SCOPE(1,
        Lang_dump(&fungus_lang);
    );

    if (argc > 1) {
        for (char **list = &argv[1]; *list; ++list)
            if (!test_file(*list, &name_table))
                break;
    } else {
        repl(&name_table);
    }

    fungus_lang_quit();
    pattern_lang_quit();
    Names_del(&name_table);
    names_quit();
    types_quit();

    return 0;
}
