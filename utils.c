#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <regex.h>
#include <errno.h>

#include "shared.h"

size_t fsize(FILE *f) {
    fseek(f, 0L, SEEK_END);
    size_t fsize = ftell(f);
    rewind(f);
    return fsize + 1; // For \0
}

void *alloc_str(const char *first, ...) {
    va_list ap;

    char *res = malloc(sizeof(char) * (strlen(first) + 1));
    memcpy(res, first, strlen(first) + 1);

    va_start(ap, first);
    char *s;

    while ( (s = va_arg(ap, char*)) != NULL ) {
        res = realloc(res, sizeof(char) * (strlen(res) + strlen(s) + 1));
        strcat(res, s);
    }
    return res;
}

void compress_spaces(char *str) {
    char *dst = str;
    for (; *str; ++str) {
        *dst++ = *str;

        if (isspace(*str)) {
            do ++str;
            while (isspace(*str));
            --str;
        }
    }
    *dst = 0;
}
