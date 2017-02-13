#define _XOPEN_SOURCE 500 // nftw
#define PORTAGE_DIRS

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ftw.h>

#include "use.h"
#include "utils.h"
#include "shared.h"

extern const char* PORTAGE_DB_DIR;

extern char *strndup(const char *, unsigned long); // It seems like gcc can't find it by itself

char* SEARCHED_PACKAGE = NULL;
int get_basic_package_info(const char *fpath, const struct stat *sb, int tflag, struct FTW *ftwbuf) {
    if (tflag == FTW_D){
        if (strncmp(fpath + ftwbuf->base, SEARCHED_PACKAGE, strlen(SEARCHED_PACKAGE)) == 0) {
            if (strstr(fpath + ftwbuf->base, SEARCHED_PACKAGE)[strlen(SEARCHED_PACKAGE)] == '-') {

                PACKAGE->name = strndup(fpath + ftwbuf->base, strlen(SEARCHED_PACKAGE)); // Shouldn't free those two 'cause they are always needed
                PACKAGE->version = strdup(fpath + ftwbuf->base + strlen(PACKAGE->name) + 1);

                char *tmpcat = alloc_str(fpath, "/CATEGORY", NULL);
                FILE *category = fopen(tmpcat, "r");
                size_t category_fsize = fsize(category);
                PACKAGE->category = malloc(sizeof(char) * category_fsize);
                fscanf(category, "%s", PACKAGE->category);
                fclose(category);
                free(tmpcat);

                char *tmprep = alloc_str(fpath, "/repository", NULL);
                FILE *repository = fopen(tmprep, "r");
                size_t repository_fsize = fsize(repository);
                PACKAGE->repository = malloc(sizeof(char) * repository_fsize);
                fscanf(repository, "%s", PACKAGE->repository);
                fclose(repository);
                free(tmprep);

                return 1;
            }
        }
    }

    return 0;
}

// Find info directory used by package
void find_package_info_dir(char* name) {
    SEARCHED_PACKAGE = strdup(name);
    nftw(PORTAGE_DB_DIR, get_basic_package_info, 100, 0);
}

int main(int argc, char** argv) {
    if (argc < 3) {
        printf("Usage: equ " ANSI_BOLD "<command> <package-name>\n" ANSI_COLOR_RESET);
        printf(ANSI_BOLD "u" ANSI_COLOR_RESET " - show use flags\n");
        return 1;
    }
    PACKAGE = malloc(sizeof(package_info_t));
    find_package_info_dir(argv[2]);
    
    if (strcmp(argv[1], "u") == 0) {
        parse_use();
    } else {
        printf("Unknown command");
    }

    return 0;
}
