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

#include <glib.h>

#include "use.h"
#include "belongs.h"
#include "meta.h"

#include "utils.h"
#include "shared.h"

extern const char *PORTAGE_DB_DIR;
extern const char *PORTAGE_EBUILDS_DIR;

char *SEARCHED_PACKAGE = NULL;
char *PACKAGE_INFO_DIR = NULL;

int search_by_partial_name(const char *fpath, const struct stat *sb, int tflag, struct FTW *ftwbuf) {
    if (tflag == FTW_D) {
        if (ftwbuf->level == 2) {
            char **path_tokens = g_strsplit(fpath + strlen(PORTAGE_DB_DIR) + 1, "/", 0);
            char *category = path_tokens[0];
            char *name = path_tokens[1];
            if (strncmp(name, SEARCHED_PACKAGE, strlen(SEARCHED_PACKAGE)) == 0) {
                char **tokens = g_strsplit(name, "-", 0);
                int v_n;
                for (int i = 0; i < g_strv_length(tokens); i++) {
                    if (isdigit(tokens[i][0])) {
                        v_n = i;
                        break;
                    }
                }
                char *version = g_strjoinv("-", tokens + v_n);
                char *pname = g_strndup(name, strlen(name) - (strlen(version) + 1));

                if (strcmp(pname, SEARCHED_PACKAGE) == 0) {
                    PACKAGE->installed = true;
                    PACKAGE->name = g_strdup(pname);
                    PACKAGE->category = g_strndup(fpath + strlen(PORTAGE_EBUILDS_DIR), strlen(fpath + strlen(PORTAGE_EBUILDS_DIR)) - (strlen(name) + 1));

                    PACKAGE->version = g_strdup(version);
                    PACKAGE_INFO_DIR = g_strdup(fpath);

                    g_strfreev(tokens);
                    free(version);
                    free(pname);

                    return 1;
                }
                g_strfreev(tokens);
                free(version);
                free(pname);
            }
            g_strfreev(path_tokens);
        }
    }

    return 0;
}

int search_by_partial_name_not_installed(const char *fpath, const struct stat *sb, int tflag, struct FTW *ftwbuf) {
    if (tflag == FTW_D && ftwbuf->level <= 2) {
        if (strcmp(fpath + ftwbuf->base, SEARCHED_PACKAGE) == 0) { // Strict name
            PACKAGE->installed = false;

            DIR *dir = opendir(fpath);
            struct dirent *el;

            unsigned int cap = 5;
            unsigned int curr_el = 0;
            GPtrArray *ebs = g_ptr_array_new();

            while ((el = readdir(dir)) != NULL) {
                if (g_str_has_suffix(el->d_name, ".ebuild")) {
                    g_ptr_array_insert(ebs, -1, el->d_name);
                }
            }

            char *eb = find_max_version(ebs, SEARCHED_PACKAGE);

            PACKAGE->name = g_strndup(eb, strlen(SEARCHED_PACKAGE)); // minus version

            PACKAGE->version = g_strndup(eb + strlen(PACKAGE->name) + 1, strlen(eb + strlen(PACKAGE->name) + 1) - 7); // len .ebuild

            PACKAGE_INFO_DIR = g_strdup(fpath);

            closedir(dir);

            g_ptr_array_free(ebs, true);

            return 1;
        }
    }
    return 0;
}

void parse_full_name(char *tname) {
    char *name = tname + 1;

    char *c = strrchr(name, '/');
    PACKAGE->category  = g_strndup(name, strlen(name) - strlen(c));

    char **tokens = g_strsplit(name, "-", 0);

    unsigned int v_s = 0;
    for (int i = 0; i < g_strv_length(tokens); i++) {
        if isdigit(tokens[i][0]) {
            v_s = i;
            break;
        }
    }

    PACKAGE->version = g_strjoinv("-", tokens + v_s);
    g_strfreev(tokens);

    PACKAGE->name = g_strndup(name + strlen(PACKAGE->category) + 1, strlen(name + strlen(PACKAGE->category) + 1) - (strlen(PACKAGE->version) + 1));

    char *ckpth = g_strconcat(PORTAGE_DB_DIR, "/", PACKAGE->category, "/", PACKAGE->name, "-", PACKAGE->version, NULL);
    if (access(ckpth, F_OK) != -1) {
        PACKAGE->installed = true;
        PACKAGE_INFO_DIR = g_strdup(ckpth);
    } else {
        PACKAGE->installed = false; // TODO
        PACKAGE_INFO_DIR = g_strconcat(PORTAGE_EBUILDS_DIR, "/", PACKAGE->category, "/", PACKAGE->name, NULL);
    }

    free(ckpth);
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
        parse_full_name(name);
    }

    if (PACKAGE->name == NULL) { // nfound
        printf("Package" ANSI_COLOR_GREEN " %s " ANSI_COLOR_RESET "not found\n", name);
        exit(0);
    }

    if (PACKAGE->installed) {
        char *rep_path = g_strconcat(PACKAGE_INFO_DIR, "/repository", NULL);

        g_file_get_contents(rep_path, &PACKAGE->repository, NULL, NULL);
        PACKAGE->repository = g_strchomp(PACKAGE->repository);
        free(rep_path);
    } else {
        PACKAGE->repository = g_strdup("gentoo"); // TODO: Handle overlays

        char *parent_dir = strrchr(PACKAGE_INFO_DIR, '/');

        PACKAGE->category = g_strndup(PACKAGE_INFO_DIR + strlen(PORTAGE_EBUILDS_DIR) + 1,
                strlen(PACKAGE_INFO_DIR + strlen(PORTAGE_EBUILDS_DIR) + 1) - strlen(parent_dir));
    }
}

int main(int argc, char **argv) {
    if (argc < 3) {
        printf(ANSI_BOLD "Usage:" ANSI_COLOR_CYAN " %s " ANSI_COLOR_GREEN "<command> <package-name>\n" ANSI_COLOR_RESET, argv[0]);

        const char *const options[]   =   { "(u)ses", "(b)elongs", "(m)eta" };
        const char *const expls[]     =   { "show use flags", "find a package <file> belongs to", "package metadata" };

        for (int i = 0; i < sizeof(options) / sizeof(char*); i++) {
            printf(ANSI_BOLD " %-15s" ANSI_COLOR_RESET "%s\n", options[i], expls[i]);
        }

        return 1;
    }

    if (strcmp(argv[1], "belongs") == 0 || strcmp(argv[1], "b") == 0) {
        belongs_to(argv[2]);
    } else {
        search_package(argv[2]);

        if (strcmp(argv[1], "uses") == 0 || strcmp(argv[1], "u") == 0) {
            parse_use();
        } else if (strcmp(argv[1], "meta") == 0 || strcmp(argv[1], "m") == 0) {
            meta();
        } else {
            printf("Unknown command\n");
        }

        free(PACKAGE->name);
        free(PACKAGE->version);
        free(PACKAGE->category);
        free(PACKAGE->repository);
        free(PACKAGE);

        free(PACKAGE_INFO_DIR);
    }

    return 0;
}
