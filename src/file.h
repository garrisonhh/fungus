#ifndef FILE_H
#define FILE_H

#include "fun_types.h"
#include "utils.h"
#include "data.h"

/*
 * Files are how errors are given context
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

#endif
