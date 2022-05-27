#include <stdio.h>
#include <stdbool.h>

#include "fungus.h"
#include "lex.h"
#include "parse.h"
#include "sema.h"
#include "fir.h"

// impl'd in compile.zig; returns success
bool try_compile_file(const File *, const Names *);

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
    pattern_lang_init();
    fungus_lang_init(&name_table);

    if (argc > 1) {
        // compile files passed in as arguments
        for (char **list = &argv[1]; *list; ++list)
            if (!test_file(*list, &name_table))
                break;
    } else {
        // run the repl if no files
        repl(&name_table);
    }

    fungus_lang_quit();
    pattern_lang_quit();
    Names_del(&name_table);
    names_quit();
    types_quit();

    return 0;
}
