#ifndef FILE_H
#define FILE_H

#include "utils.h"
#include "data.h"

/*
 * Files are the context for tokens and smart errors
 */

typedef struct File {
    const char *filepath;
    View text;

    // a list of char indices for the beginning of each line
    hsize_t *lines;
    size_t lines_len;

    // flags
    bool owns_text;
} File;

File File_open(const char *filepath);
File File_read_stdin(void);
File File_from_str(const char *filepath, const char *str, size_t len);
void File_del(File *);

// for zig interop
const char *File_str(const File *);
size_t File_len(const File *);

// print out locations in a file
void File_display_at(FILE *, const File *, hsize_t start, hsize_t len);
void File_display_from(FILE *, const File *, hsize_t start);

void File_verror_at(const File *, hsize_t start, hsize_t len, const char *fmt,
                    va_list args);
void File_verror_from(const File *, hsize_t start, const char *fmt,
                      va_list args);

void File_error_at(const File *, hsize_t start, hsize_t len, const char *fmt,
                   ...);
void File_error_from(const File *, hsize_t start, const char *fmt, ...);

static inline hsize_t File_eof(const File *f) { return f->text.len; }

#endif
