#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>

#include "shared.h"


#include <glib.h>

extern package_info_t *PACKAGE;
extern const char *EMERGE_LOG_FILE;

void merge_time(char *name) {
    FILE *lgf = fopen(EMERGE_LOG_FILE, "r");

    if (lgf == NULL) {
        printf("Can't open %s, are you a member of the portage group?\n", EMERGE_LOG_FILE);
        fclose(lgf);
        return;
    }


    char *line;
    size_t len;
    
    bool searching_for_end = false;
    time_t start_timestamp, end_timestamp;
    char *found_pname;

    while (getline(&line, &len, lgf) != -1) {
        if (searching_for_end) {
            
            char curr_name[64];
            
            if (sscanf(line, "%li:  ::: completed %*[^)]) %s to %*s", &end_timestamp, curr_name) != 2)
                continue;

            if (strcmp(curr_name, found_pname) == 0) {
                char *st = asctime(localtime(&end_timestamp));
                st[strlen(st) - 1] = '\0';

                printf("%s >>> " ANSI_COLOR_GREEN ANSI_BOLD "%s\n" ANSI_COLOR_RESET
                        "  merge time: %.2f minutes\n\n", st, found_pname, (end_timestamp - start_timestamp) / 60.0);

                //printf("%li\n",(end_timestamp - start_timestamp));
                searching_for_end = false;
                //exit(0);
            }
        } else {
            char package_name[64];

            if ((sscanf(line, "%li:  >>> emerge %*[^)]) %s to %*s", &start_timestamp, package_name)) != 2) {
                continue;
            }

            char *nm_w_ver = g_strdup(strchr(package_name, '/') + 1);
            char **tokens = tokens = g_strsplit(nm_w_ver, "-", 0);
            char *rlname = NULL;

            for (int i = 0; i < g_strv_length(tokens); i++) {
                if (isdigit(tokens[i][0])) {
                    rlname = g_strndup(nm_w_ver, strlen(nm_w_ver) - (strlen(tokens[i]) + 1));
                    break;
                }
            }

            if (strcmp(rlname, name) != 0) {
                free(nm_w_ver);
                free(rlname);
                g_strfreev(tokens);
                continue;
            }


            searching_for_end = true;
            found_pname = package_name;

            
            free(nm_w_ver);
            free(rlname);
            g_strfreev(tokens);        
        }
    }
    free(line);

    fclose(lgf);
}
