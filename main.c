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

#include "sds.h"

extern const char *PORTAGE_DB_DIR;
extern const char *PORTAGE_EBUILDS_DIR;

extern char *strndup(const char *, unsigned long); // It seems like gcc can't find it by itself

char *SEARCHED_PACKAGE = NULL;
char *PACKAGE_INFO_DIR = NULL;

int search_by_partial_name(const char *fpath, const struct stat *sb, int tflag, struct FTW *ftwbuf) {
    if (tflag == FTW_D){
        if (strncmp(fpath + ftwbuf->base, SEARCHED_PACKAGE, strlen(SEARCHED_PACKAGE)) == 0) {
            sds name = sdsnew(basename((char *)fpath));
            sds version = sdsempty();

            sds *tokens;
            int count;

            bool was = false;
            tokens = sdssplitlen(name, sdslen(name), "-", 1, &count);
            for (int i = 1; i < count; i++) {
                if isdigit(tokens[i][0])
                    was = true;
                if (was == true)
                    version = sdscatprintf(version, "-%s", tokens[i]);
            }
            sdsfreesplitres(tokens, count);

            sdsrange(version, 1, -1);
            sdsrange(name, 0, -(sdslen(version) + 2));

            if (strcmp(name, SEARCHED_PACKAGE) == 0) {
                PACKAGE->installed = true;

                PACKAGE->name = name;

                PACKAGE->category = sdsnew(fpath);
                sdsrange(PACKAGE->category, strlen(PORTAGE_EBUILDS_DIR), -(strlen(basename((char *)fpath)) + 2));

                PACKAGE->version = version;
                PACKAGE_INFO_DIR = sdsnew(fpath);

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

            unsigned int cap = 5;
            unsigned int curr_el = 0;
            sds *ebs = malloc(sizeof(sds) * cap);

            while ((el = readdir(dir)) != NULL) {
                if (strstr(el->d_name, ".ebuild") != NULL) {
                    if (++curr_el > cap) {
                        cap *= 2;
                        sds *tmp = realloc(ebs, sizeof(sds) * cap);
                        if (tmp) {
                            ebs = tmp;
                            ebs[curr_el - 1] = sdsnew(el->d_name);
                        } else {
                            free(ebs);
                            perror("Cannot realloc ebuild array");
                            exit(1);
                        }
                    } else {
                        ebs[curr_el - 1] = sdsnew(el->d_name);
                    }
                }
            }

            sds eb = find_max_version(ebs, SEARCHED_PACKAGE, curr_el);

            PACKAGE->name = sdsempty();
            PACKAGE->name = sdscpy(PACKAGE->name, eb);
            sdsrange(PACKAGE->name, 0, strlen(SEARCHED_PACKAGE) - 1); // minus version

            PACKAGE->version = sdsempty();
            PACKAGE->version = sdscpy(PACKAGE->version, eb);
            sdsrange(PACKAGE->version, strlen(PACKAGE->name) + 1, -(7 + 1)); // len .ebuild + end of str

            PACKAGE_INFO_DIR = sdsnew(fpath);

            closedir(dir);

            for(int i = 0; i < curr_el; i++)
                sdsfree(ebs[i]);
            free(ebs);

            return 1;
        }
    }
    return 0;
}

void search_package(sds name) {
    SEARCHED_PACKAGE = name;
    PACKAGE = malloc(sizeof(package_info_t));
    PACKAGE->name = NULL; // To check later

    if (name[0] != '=') { // Partial name
        nftw(PORTAGE_DB_DIR, search_by_partial_name, 100, 0); // Search installed
        if (PACKAGE->name == NULL)
            nftw(PORTAGE_EBUILDS_DIR, search_by_partial_name_not_installed, 100, 0);
    } else { // =category/name-version
        sdsrange(name, 1, -1);

        sds category = sdsdup(name);
        char *c = strrchr(category, '/');
        sdsrange(category, 0, -(strlen(c) + 1));


        PACKAGE->category = sdsempty();
        PACKAGE->category = sdscpy(PACKAGE->category, category);

        sds *tokens;
        int count;

        bool was = false;
        tokens = sdssplitlen(name, sdslen(name), "-", 1, &count);
        sds version = sdsempty();
        for (int i = 1; i < count; i++) {
            if isdigit(tokens[i][0])
                was = true;
            if (was == true)
                version = sdscatprintf(version, "-%s", tokens[i]);
        }
        sdsfreesplitres(tokens, count);

        sdsrange(version, 1, -1);
        sdsrange(name, 0, -(sdslen(version) + 2));

        sds pname = sdsdup(name);
        sdsrange(pname, strlen(category) + 1, -1);

        PACKAGE->name = sdsempty();
        PACKAGE->name = sdscpy(PACKAGE->name, pname);
        PACKAGE->version = sdsempty();
        PACKAGE->version = sdscpy(PACKAGE->version, version);

        char *ckpth = alloc_str(PORTAGE_DB_DIR, "/", PACKAGE->category, "/", PACKAGE->name, "-", PACKAGE->version, NULL);
        if (access(ckpth, F_OK) != -1) {
            PACKAGE->installed = true;
            PACKAGE_INFO_DIR = sdsdup(ckpth);
        } else {
            PACKAGE->installed = false; // TODO
            PACKAGE_INFO_DIR = sdsempty();
            PACKAGE_INFO_DIR = sdscatprintf(PACKAGE_INFO_DIR, "%s/%s/%s", PORTAGE_EBUILDS_DIR, PACKAGE->category, PACKAGE->name);
        }

        sdsfree(ckpth);
    }

    if (PACKAGE->name == NULL) { // nfound
        printf("Package" ANSI_COLOR_GREEN " %s " ANSI_COLOR_RESET "not found\n", name);
        exit(0);
    }

    if (PACKAGE->installed) {
        sds tmprep = alloc_str(PACKAGE_INFO_DIR, "/repository", NULL);
        FILE *repository = fopen(tmprep, "r");

        char *tmpcr = read_file_content(repository);
        PACKAGE->repository = sdsnew(tmpcr);

        sdstrim(PACKAGE->repository, "\n\t ");
        fclose(repository);

        sdsfree(tmprep);
        free(tmpcr);
    } else {
        PACKAGE->repository = sdsnew("gentoo"); // TODO: Handle overlays

        char *tmpparent = strrchr(PACKAGE_INFO_DIR, '/');
        sds parent = sdsnew(PACKAGE_INFO_DIR);
        sdsrange(parent, 0, -(strlen(tmpparent) + 1));

        PACKAGE->category = sdsdup(parent);

        sdsrange(PACKAGE->category, strlen(PORTAGE_EBUILDS_DIR) + 1, -1);
    }
}

int main(int argc, char **argv) {
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

    sdsfree(PACKAGE->name);
    sdsfree(PACKAGE->version);
    sdsfree(PACKAGE->category);
    sdsfree(PACKAGE->repository);
    free(PACKAGE);

    sdsfree(PACKAGE_INFO_DIR);

    return 0;
}
