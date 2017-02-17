#define _XOPEN_SOURCE 500 // nftw
#define _GNU_SOURCE 700 // getline

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <libgen.h>
#include <ftw.h>

#include "sds.h"

#include "shared.h"
#include "utils.h"

extern const char* PORTAGE_DB_DIR;

char *BELONGS_TO_SEARCHED_NAME;
int belongs_to_find(const char *fpath, const struct stat *sb, int tflag, struct FTW *ftwbuf) {
    if (tflag == FTW_D && ftwbuf->level == 2) {
        sds tmppth = alloc_str(fpath, "/CONTENTS", NULL);
        FILE *fs = fopen(tmppth, "r");
        if (fs == NULL)
            return 0;

        char *line = NULL;
        size_t len;

        while (getline(&line, &len, fs) != -1) {
            if (strstr(basename(line), BELONGS_TO_SEARCHED_NAME)) {
                sds tm = sdsnew(line);
                sdsrange(tm, 0, -2); // no newline

                int count;
                sds *tokens = sdssplitlen(tm, sdslen(tm), " ", 1, &count);
                sds nd = sdsdup(tokens[1]);

                sds tmpp = sdsnew(fpath);
                sdsrange(tmpp, strlen(PORTAGE_DB_DIR) + 1, -1);

                printf(ANSI_BOLD ANSI_COLOR_GREEN "%s "ANSI_COLOR_RESET ANSI_BOLD "(%s)\n", tmpp, nd);
                sdsfree(tm);
            }
        }

        fclose(fs);
        sdsfree(tmppth);
        free(line);
    }
    return 0;
}

void belongs_to(char *fname) {
    BELONGS_TO_SEARCHED_NAME = fname;

    printf("Searching for" ANSI_BOLD " %s...\n" ANSI_COLOR_RESET, fname);
    nftw(PORTAGE_DB_DIR, belongs_to_find, 100, 0);
}
