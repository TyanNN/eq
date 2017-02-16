#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <regex.h>
#include <stdarg.h>

#include "shared.h"

#include "semver.h"
#include "sds.h"

char *find_max_version(sds *arr, sds package_name, unsigned int n) {
    semver_t max = {};
    unsigned int maxi;

    for (int i = 0; i < n; i++) {
        sds name = sdsdup(arr[i]);

        char *c;
        if ((c = strrchr(name, '.')) != NULL) // if has extension
            sdsrange(name, 0, -(strlen(c) + 1));


        sds version = sdsdup(name);
        sdsrange(version, strlen(package_name) + 1, -1);

        semver_t curr = {};
        semver_parse(version, &curr);

        if (semver_gt(curr, max)) {
            max = curr;
            maxi = i;
        }
        sdsfree(name);
        sdsfree(version);
    }

    return arr[maxi];
}

// Allocate a string and read to it from file
char *read_file_content(FILE *f) {
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

sds alloc_str(const sds first, ...) {
    va_list ap;

    sds res = sdsnew(first);

    va_start(ap, first);
    char *s;

    while ( (s = va_arg(ap, sds)) != NULL )
        res = sdscat(res, s);

    va_end(ap);
    return res;
}

void compress_spaces(char *s) {
    char *d = s;
    do while(*s == '\t' || (isspace(*s) && isspace(*(s + 1)))) {s++; *s=' ';} while((*d++ = *s++));
}
