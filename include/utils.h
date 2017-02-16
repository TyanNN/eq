#ifndef H_EQU_UTILS
#define H_EQU_UTILS

#include <stdio.h>

char *find_max_version(char **arr, char *package_name, unsigned int n);

char *read_file_content(FILE* f);

// Allocate a string, must end with NULL
void *alloc_str(const char *first, ...);

// http://stackoverflow.com/questions/1217721/how-do-i-replace-multiple-spaces-with-a-single-space
void compress_spaces(char *str);

#endif
