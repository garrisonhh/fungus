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
        .text = read_file(filepath),
        .owns_text = true
    };

    get_file_lines(&f);

    return f;
}

File File_read_stdin(void) {
    File f = { .filepath = "stdin", .owns_text = true };

    // read lines from stdin until an empty line is read
#define FGETS_BUFSIZE 4096
    char buf[FGETS_BUFSIZE] = {0};
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

File File_from_str(const char *filepath, const char *str, size_t len) {
   File f = {
       .filepath = filepath,
       .text = { str, len }
   };

   get_file_lines(&f);

   return f;
}

void File_del(File *f) {
    if (f->owns_text)
        free((char *)f->text.str);

    free(f->lines);
}

// locations ===================================================================

typedef struct FileLoc {
    int lineno, charno;
} FileLoc;

#define UP_ARROW   (TC_CYAN "^" TC_RESET)
#define DOWN_ARROW (TC_CYAN "v" TC_RESET)

static FileLoc loc_of(const File *f, hsize_t idx) {
    FileLoc loc = { .lineno = f->lines_len };

    for (size_t i = 0; i < f->lines_len; ++i) {
        if (idx < f->lines[i]) {
            loc.lineno = i;
            break;
        }
    }

    loc.charno = idx - f->lines[loc.lineno - 1] + 1;

    return loc;
}

// amount of characters needed to display a lineno range
static int lineno_chars(int max_no) {
    int n = 0;

    while (max_no > 0) {
        max_no /= 10;
        ++n;
    }

    return MAX(n, 1);
}

static void display_line(FILE *fp, const File *file, int lineno, int ln_chars) {
    // get line length
    const char *line = &file->text.str[file->lines[lineno - 1]];
    size_t len = 0;

    while (line[len] && line[len] != '\n')
        ++len;

    fprintf(fp, TC_DIM " %*d | " TC_RESET "%.*s\n",
            ln_chars, lineno, (int)len, line);
}

static void display_arrow(FILE *fp, int charno, int ln_chars, const char *arw) {
    fprintf(fp, TC_DIM " %*s |" TC_RESET "%*s%s\n",
            ln_chars, "", charno, "", arw);
}

void File_display_at(FILE *fp, const File *file, hsize_t start, hsize_t len) {
    FileLoc start_loc = loc_of(file, start);
    FileLoc end_loc = loc_of(file, start + (len ? len - 1 : 0));
    int ln_chars = lineno_chars(MIN(end_loc.lineno + 1, file->lines_len));

    if (start_loc.lineno > 1)
        display_line(fp, file, start_loc.lineno - 1, ln_chars);

    display_arrow(fp, start_loc.charno, ln_chars, DOWN_ARROW);

    for (int ln = start_loc.lineno; ln <= end_loc.lineno; ++ln)
        display_line(fp, file, ln, ln_chars);

    display_arrow(fp, end_loc.charno, ln_chars, UP_ARROW);

    if (end_loc.lineno < file->lines_len)
        display_line(fp, file, end_loc.lineno + 1, ln_chars);
}

void File_display_from(FILE *fp, const File *file, hsize_t start) {
    FileLoc loc = loc_of(file, start);
    int ln_chars = lineno_chars(MIN(loc.lineno + 1, file->lines_len));

    if (loc.lineno > 1)
        display_line(fp, file, loc.lineno - 1, ln_chars);

    display_line(fp, file, loc.lineno, ln_chars);
    display_arrow(fp, loc.charno, ln_chars, UP_ARROW);

    if (loc.lineno < file->lines_len)
        display_line(fp, file, loc.lineno + 1, ln_chars);
}

static void File_error_msg(const File *f, FileLoc loc, const char *fmt,
                           va_list args) {
    // print loc
    fprintf(stderr,
            TC_DIM "%s:%d:%d " TC_RESET "[" TC_RED "ERROR" TC_RESET "] ",
            f->filepath, loc.lineno, loc.charno);

    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
}

void File_verror_at(const File *f, hsize_t start, hsize_t len, const char *fmt,
                    va_list args) {
    File_error_msg(f, loc_of(f, start), fmt, args);
    File_display_at(stderr, f, start, len);
}

void File_verror_from(const File *f, hsize_t start, const char *fmt,
                      va_list args) {
    File_error_msg(f, loc_of(f, start), fmt, args);
    File_display_from(stderr, f, start);
}

void File_error_at(const File *f, hsize_t start, hsize_t len, const char *fmt,
                   ...) {
    va_list args;

    va_start(args, fmt);
    File_verror_at(f, start, len, fmt, args);
    va_end(args);
}

void File_error_from(const File *f, hsize_t start, const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    File_verror_from(f, start, fmt, args);
    va_end(args);
}
