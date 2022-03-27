#ifndef FILE_H
#define FILE_H

#include "fun_types.h"
#include "utils.h"
#include "data.h"

/*
 * Files are the context for tokens and smart errors
 */

typedef struct File {
    const char *filepath;

    // a list of char indices for the beginning of each line
    hsize_t *lines;
    size_t lines_len;

    View text;
} File;

File File_open(const char *filepath);
File File_read_stdin(void);
void File_del(File *);

// print out locations in a file
void File_display_at(FILE *, const File *, hsize_t start, hsize_t len);
void File_display_from(FILE *, const File *, hsize_t start);

void File_error_at(const File *, hsize_t start, hsize_t len, const char *fmt,
                   ...);
void File_error_from(const File *, hsize_t start, const char *fmt, ...);

#endif
