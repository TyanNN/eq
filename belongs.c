#define _XOPEN_SOURCE 500 // nftw
#define _GNU_SOURCE 700 // getline

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <libgen.h>
#include <ftw.h>

#include <glib.h>

#include "shared.h"
#include "utils.h"

extern const char* PORTAGE_DB_DIR;

char *BELONGS_TO_SEARCHED_NAME;
int belongs_to_find(const char *fpath, const struct stat *sb, int tflag, struct FTW *ftwbuf) {
    if (tflag == FTW_D && ftwbuf->level == 2) {
        char *tmppth = g_strconcat(fpath, "/CONTENTS", NULL);
        FILE *fs = fopen(tmppth, "r");
        if (fs == NULL)
            return 0;

        char *line = NULL;
        size_t len;

        while (getline(&line, &len, fs) != -1) {
            if (strstr(basename(line), BELONGS_TO_SEARCHED_NAME)) {
                char *tm = g_strndup(line, strlen(line) - 1); // no newline

                char **tokens = g_strsplit(line, " ", 0);
                char *nd = tokens[1];

                char *tmpp = g_strdup(fpath + strlen(PORTAGE_DB_DIR) + 1);

                printf(ANSI_BOLD ANSI_COLOR_GREEN "%s "ANSI_COLOR_RESET ANSI_BOLD "(%s)\n", tmpp, nd);

                g_strfreev(tokens);
                free(tm);
                free(tmpp);
            }
        }

        fclose(fs);
        free(tmppth);
        free(line);
    }
    return 0;
}

void belongs_to(char *fname) {
    BELONGS_TO_SEARCHED_NAME = fname;

    printf("Searching for" ANSI_BOLD " %s...\n" ANSI_COLOR_RESET, fname);
    nftw(PORTAGE_DB_DIR, belongs_to_find, 100, 0);
}
