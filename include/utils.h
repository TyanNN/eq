#ifndef H_EQU_UTILS
#define H_EQU_UTILS

#include <stdio.h>
#include <glib.h>

char *find_max_version(GPtrArray *arr, char *package_name);

char *read_file_content(FILE* f);

// http://stackoverflow.com/questions/1217721/how-do-i-replace-multiple-spaces-with-a-single-space
void compress_spaces(char *str);

#endif
