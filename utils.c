#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <regex.h>
#include <stdarg.h>

#include "shared.h"

#include <glib.h>

#include "semver.h"

char *find_max_version(GPtrArray *arr, char *package_name) {
    semver_t max = {};
    unsigned int maxi;

    char *name;
    for (int i = 0; i < arr->len; i++) {
        char *el = g_ptr_array_index(arr, i++);
        char *has_ext = strrchr(el, '.');

        if (has_ext) {
            name = g_strndup(el, strlen(el) - strlen(has_ext));
        } else {
            name = g_strdup(el);
        }
        char *version = g_strdup(name + strlen(package_name) + 1);

        semver_t curr = {};
        semver_parse(version, &curr);

        if (semver_gt(curr, max)) {
            max = curr;
            maxi = i;
        }

        free(name);
        free(version);
    }

    return g_ptr_array_index(arr, maxi);
}

// Allocate a string and read to it from file
char *read_file_content(FILE *f) {
    printf("REPLACE WITH GLIB FUNC");
    exit(1);
    fseek(f, 0L, SEEK_END);
    long long fsize = ftell(f);
    rewind(f);
    char *ret = calloc(fsize + 1, sizeof(char));

    if (!ret) {
        perror("Memory allocation for file failed in read_file_content");
        printf("Trying to allocate %lli bytes\n", fsize);
        exit(1);
    }

    fread(ret, 1, fsize, f);
    return ret;
}

void compress_spaces(char *s) {
    char *d = s;
    do while(*s == '\t' || (isspace(*s) && isspace(*(s + 1)))) {s++; *s=' ';} while((*d++ = *s++));
}
