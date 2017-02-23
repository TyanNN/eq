#include <stdio.h>
#include <stdlib.h>

#include <glib.h>

#include "shared.h"

extern package_info_t *PACKAGE;

static double bytes_to_mb(long bytes) {
    return bytes / 1024.0 / 1024.0;
}

void size() {
    if (!PACKAGE->installed) {
        printf("Package %s is not installed, can't get size", PACKAGE->name);
    }

    printf(ANSI_COLOR_GREEN " %s/%s-%s\n" ANSI_COLOR_RESET, PACKAGE->category, PACKAGE->name, PACKAGE->version);
    char *path = g_strconcat(PACKAGE->info_dir, "/SIZE", NULL);
    unsigned long int size;

    FILE *size_file = fopen(path, "r");
    fscanf(size_file, "%lu", &size);
    fclose(size_file);

    char *path_cont = g_strconcat(PACKAGE->info_dir, "/CONTENTS", NULL);
    char *cont;
    g_file_get_contents(path_cont, &cont, NULL, NULL);

    int i;
    for (i=0; cont[i]; cont[i] == '\n' ? i++ : *cont++);

    printf("  %-12s\t" ANSI_BOLD ANSI_COLOR_CYAN "%i\n" ANSI_COLOR_RESET, "Total files:", i);
    printf("  %-12s\t" ANSI_BOLD ANSI_COLOR_CYAN "%.2fMB\n" ANSI_COLOR_RESET, "Total size:", bytes_to_mb(size));

    free(path);
    free(path_cont);
}
