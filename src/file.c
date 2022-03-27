#include <stdio.h>

#include "file.h"

static View read_file(const char *filepath) {
    FILE *fp = fopen(filepath, "r");

    if (!fp)
        fungus_panic("could not open file \"%s\"!", filepath);

    fseek(fp, 0, SEEK_END);

    size_t len = ftell(fp);
    char *text = malloc(len + 1);

    text[len] = '\0';
    rewind(fp);
    fread(text, 1, len, fp);

    fclose(fp);

    return (View){ text, len };
}

static void get_file_lines(File *f) {
#define INIT_LINES_CAP 32
    hsize_t cap = INIT_LINES_CAP;

    f->lines = malloc(cap * sizeof(*f->lines));
    f->lines_len = 1;
    f->lines[0] = 0;

    // count
    const View *text = &f->text;

    for (hsize_t i = 1; i < text->len; ++i) {
        if (text->str[i - 1] != '\n')
            continue;

        // push a line #
        f->lines[f->lines_len++] = i;

        if (f->lines_len == cap) {
            cap *= 2;
            f->lines = realloc(f->lines, cap * sizeof(*f->lines));
        }
    }
}

File File_open(const char *filepath) {
    File f = {
        .filepath = filepath,
        .text = read_file(filepath)
    };

    get_file_lines(&f);

    return f;
}

File File_read_stdin(void) {
    File f = { .filepath = "stdin" };

    // read lines from stdin until an empty line is read
#define FGETS_BUFSIZE 4096
    char buf[FGETS_BUFSIZE];
    char *str = NULL;
    size_t len = 0;

    while (true) {
        if (!str)
            printf(">>> ");
        else
            printf("    ");

        fgets(buf, FGETS_BUFSIZE, stdin);

        size_t read_len = strlen(buf);

        if (read_len <= 1)
            break;

        str = realloc(str, (len + read_len + 1) * sizeof(*str));

        strncpy(&str[len], buf, read_len);
        len += read_len;
    }

    // store read data
    f.text = (View){ str, len };

    get_file_lines(&f);

    return f;
}

void File_del(File *f) {
    free((char *)f->text.str);
    free(f->lines);
}
