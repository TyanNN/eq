#define _XOPEN_SOURCE 500 // nftw
#define PORTAGE_DIRS

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ftw.h>
#include <dirent.h>
#include <ctype.h>
#include <libgen.h>
#include <unistd.h>

#include "use.h"
#include "utils.h"
#include "shared.h"

extern const char* PORTAGE_DB_DIR;
extern const char* PORTAGE_EBUILDS_DIR;

extern char *strndup(const char *, unsigned long); // It seems like gcc can't find it by itself

char* SEARCHED_PACKAGE = NULL;
char *PACKAGE_INFO_DIR = NULL;

int search_by_partial_name(const char *fpath, const struct stat *sb, int tflag, struct FTW *ftwbuf) {
    if (tflag == FTW_D){
        if (strncmp(fpath + ftwbuf->base, SEARCHED_PACKAGE, strlen(SEARCHED_PACKAGE)) == 0) {
            const char *dirname = basename((char *)fpath);
            const char *search = dirname;
            while((search = strchr(search, '-')) != NULL) {
                if (isdigit((search + 1)[0])) break;
                search = strchr(++search, '-');
            }
            const char *version = search + 1;

            char *realname = strdup(dirname);
            realname[strlen(dirname) - strlen(search)] = '\0';
            if (strcmp(realname, SEARCHED_PACKAGE) == 0) {
                PACKAGE->installed = true;
                PACKAGE->name = strdup(realname);
                PACKAGE->version = strdup(version);

                PACKAGE_INFO_DIR = strdup(fpath);
                
                free(realname);

                return 1;
            }
        }
    }

    return 0;
}

int search_by_partial_name_not_installed(const char *fpath, const struct stat *sb, int tflag, struct FTW *ftwbuf) {
    if (tflag == FTW_D && ftwbuf->level <= 2){
        if (strcmp(fpath + ftwbuf->base, SEARCHED_PACKAGE) == 0) { // Strict name
            PACKAGE->installed = false;

            DIR *dir = opendir(fpath);
            struct dirent *el;
            while ((el = readdir(dir)) != NULL) {
                if (strlen(el->d_name) > 6) { // smth.ebuild
                    char *c = strrchr(el->d_name, '.');
                    if (c != NULL && strcmp(c, ".ebuild") == 0) {
                        char *tmpvr = el->d_name + strlen(fpath + ftwbuf->base) + 1;
                        tmpvr[strlen(tmpvr) - 7] = '\0';  // Find version
                        PACKAGE->name = strdup((char*)(fpath + ftwbuf->base));
                        PACKAGE->version = strdup(tmpvr);
                        PACKAGE_INFO_DIR = strdup(fpath);

                        return 1;
                    }
                }
            }
        }
    }
    return 0;
}

void search_package(char *name) {
    SEARCHED_PACKAGE = name;
    PACKAGE = malloc(sizeof(package_info_t));
    PACKAGE->name = NULL; // To check later

    if (name[0] != '=') { // Partial name
        nftw(PORTAGE_DB_DIR, search_by_partial_name, 100, 0); // Search installed
        if (PACKAGE->name == NULL)
            nftw(PORTAGE_EBUILDS_DIR, search_by_partial_name_not_installed, 100, 0);
    } else { // =category/name-version
        name = name + 1;
        
        const char *category = strdup(name);
        char *c = strrchr(category, '/');
        *c = '\0';
        PACKAGE->category = strdup(category);

        const char *search = name;

        while((search = strchr(search, '-')) != NULL) {
            if (isdigit((search + 1)[0])) break;
            search = strchr(++search, '-');
        }
        
        const char* version = search + 1;
        char *pname = strdup(name) + strlen(category) + 1;
        pname[strlen(pname) - strlen(version) - 1] = '\0';
        PACKAGE->name = strdup(pname);
        PACKAGE->version = strdup(version);

        char *ckpth = alloc_str(PORTAGE_DB_DIR, "/", PACKAGE->category, "/", PACKAGE->name, "-", PACKAGE->version, NULL);
        if (access(ckpth, F_OK) != -1) {
            PACKAGE->installed = true;
            PACKAGE_INFO_DIR = strdup(ckpth);
        } else {
            PACKAGE->installed = false; // TODO
            PACKAGE_INFO_DIR = "";
        }

        //free(pname); this segfaults
        free(ckpth);
    }

    if (PACKAGE->name == NULL) { // found
        printf("Package" ANSI_COLOR_GREEN " %s " ANSI_COLOR_RESET "not found\n", name);
        exit(0);
    }

    if (PACKAGE->installed) {
        char *tmpcat = alloc_str(PACKAGE_INFO_DIR, "/CATEGORY", NULL);
        FILE *category = fopen(tmpcat, "r");
        size_t category_fsize = fsize(category);
        PACKAGE->category = malloc(sizeof(char) * category_fsize);
        fscanf(category, "%s", PACKAGE->category);
        fclose(category);
        free(tmpcat);

        char *tmprep = alloc_str(PACKAGE_INFO_DIR, "/repository", NULL);
        FILE *repository = fopen(tmprep, "r");
        size_t repository_fsize = fsize(repository);
        PACKAGE->repository = malloc(sizeof(char) * repository_fsize);
        fscanf(repository, "%s", PACKAGE->repository);
        fclose(repository);
        free(tmprep);
    } else {
        PACKAGE->repository = "gentoo"; // TODO: Handle overlays

        char *tmpparent = strrchr(PACKAGE_INFO_DIR, '/');
        *tmpparent = '\0';

        PACKAGE->category = strdup(PACKAGE_INFO_DIR + strlen(PORTAGE_EBUILDS_DIR) + 1);
    }
}

int main(int argc, char** argv) {
    if (argc < 3) {
        printf("Usage: %s " ANSI_BOLD "<command> <package-name>\n" ANSI_COLOR_RESET, argv[0]);
        printf(ANSI_BOLD "u" ANSI_COLOR_RESET " - show use flags\n");
        return 1;
    }
    search_package(argv[2]);

    if (strcmp(argv[1], "uses") == 0 || strcmp(argv[1], "u") == 0) {
        parse_use();
    } else {
        printf("Unknown command");
    }

    return 0;
}
