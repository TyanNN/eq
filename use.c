#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <libgen.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <ctype.h>

#include <glib.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>

#include "utils.h"
#include "shared.h"

extern const char *PORTAGE_DB_DIR;
extern const char *PORTAGE_EBUILDS_DIR;
extern const char *LAYMAN_EBUILDS_DIR;

GHashTable *ALL_USE;

void parse_desc(char *path) {
    FILE *f = fopen(path, "r");
    size_t filesize = fsize(f);
    char *st = malloc(sizeof(char) * filesize);

    fread(st, 1, filesize, f);
    fclose(f);

    char *line = strtok(st, "\n");
    while (line != NULL) {
        char *use_flag = malloc(sizeof(char) * 64); // Can't really know these two, so just take some
        char *use_text = malloc(sizeof(char) * 512); // Don't deallocate, used later

        sscanf(line, "%s - %[^\n\t\v]", use_flag, use_text);

        if (use_flag[0] != '#') { // Ignore comments at the beggining of the file
            if (strstr(path, "use.desc") == NULL) { // We are not in use.desc
                char *base = basename(path);
                char *bfname = malloc(sizeof(char) * (strlen(base) - 5 + 1));
                sscanf(base, "%[^.].%*s", bfname);

                char *tmpstr = alloc_str(bfname, "_", use_flag, NULL);
                use_flag = strdup(tmpstr);
                free(tmpstr);
            }

            g_hash_table_insert(ALL_USE, use_flag, use_text);
        }

        line = strtok(NULL, "\n");
    }
    free(st);
}

int find_active_use(const void *el, const void *tofind) {
    if (strcmp((char *)el, (char *)tofind) == 0) {
        return 0;
    } else {
        return 1;
    }
}

// TODO: do something with use expand
void parse_use() {
    ALL_USE = g_hash_table_new(g_str_hash, g_str_equal);

    GSList *active_use = NULL;
    GSList *active_iuse = NULL;

    //Global use flags descriptions
    parse_desc("/usr/portage/profiles/use.desc");

    // Other descriptions
    DIR *desc_dir = opendir("/usr/portage/profiles/desc");
    struct dirent *el;
    while ((el = readdir(desc_dir)) != NULL) {
        char *fullpath = alloc_str("/usr/portage/profiles/desc/", el->d_name, NULL);
        parse_desc(fullpath);
        free(fullpath);
    }

    char *dir;
    if (strcmp(PACKAGE->repository, "gentoo") == 0) {
        dir = alloc_str(PORTAGE_EBUILDS_DIR, NULL); // From portage tree
    } else {
        dir = alloc_str(LAYMAN_EBUILDS_DIR, "/", PACKAGE->repository, NULL); // From overlay
    }

    // Per-package use flags descriptions
    char *pth = alloc_str(dir, "/", PACKAGE->category, "/", PACKAGE->name, "/metadata.xml", NULL);
    xmlDocPtr doc;

    if (access(pth, F_OK) != -1) {
        doc = xmlParseFile(pth);
    }

    free(pth);
    free(dir);

    if (PACKAGE->installed) {
        char *active_use_pth = alloc_str(PORTAGE_DB_DIR, "/", PACKAGE->category, "/", PACKAGE->name, "-", PACKAGE->version, "/USE", NULL);
        char *active_iuse_pth = alloc_str(PORTAGE_DB_DIR, "/", PACKAGE->category, "/", PACKAGE->name, "-", PACKAGE->version, "/IUSE", NULL);

        FILE *active_use_file = fopen(active_use_pth, "r");
        FILE *active_iuse_file = fopen(active_iuse_pth, "r");

        size_t active_use_fsize = fsize(active_use_file);
        size_t active_iuse_fsize = fsize(active_iuse_file);

        char *active_use_s = malloc(sizeof(char) * active_use_fsize); // Don't clean, will be freed by glib
        char *active_iuse_s = malloc(sizeof(char) * active_iuse_fsize);

        fread(active_use_s, 1, active_use_fsize, active_use_file);
        fread(active_iuse_s, 1, active_iuse_fsize, active_iuse_file);

        fclose(active_use_file);
        fclose(active_iuse_file);

        char *token = strtok(active_use_s, " ");
        while (token != NULL) {
            active_use = g_slist_append(active_use, token);
            token = strtok(NULL, " ");
        }
        free(active_use_pth);

        char *itoken = strtok(active_iuse_s, " ");
        while (itoken != NULL) {
            active_iuse = g_slist_append(active_iuse, itoken);
            itoken = strtok(NULL, " ");
        }
        free(active_iuse_pth);
    } else {
        char *eb_path;
        if (strcmp(PACKAGE->repository, "gentoo") == 0) {
            eb_path = alloc_str(PORTAGE_EBUILDS_DIR, "/", PACKAGE->category, "/", PACKAGE->name, "/", PACKAGE->name, "-", PACKAGE->version, ".ebuild", NULL);
        }
        FILE *eb_f = fopen(eb_path, "r");
        size_t eb_fsize = fsize(eb_f);
        char *eb_s = malloc(sizeof(char) * eb_fsize);
        fread(eb_s, 1, eb_fsize, eb_f);
        fclose(eb_f);

        char *token = strstr(eb_s, "IUSE=");
        while (token != NULL) {
            char iuses[256];
            sscanf(token + 5, "\"%[^\"]", iuses);
            char *c;
            while ((c = strchr(iuses, '\t')) != NULL) { // Because tabs, yeah
                *c = ' ';
            }
            compress_spaces(iuses);

            if (isalnum(iuses[0]) || iuses[0] == '+' || iuses[0] == '-') {
                char *tt = strtok(iuses, " ");
                while (tt != NULL) {
                    active_iuse = g_slist_append(active_iuse, tt);
                    tt = strtok(NULL, " ");
                }
            }

            token = strstr(token + strlen(token), "IUSE");
        }
    }

    xmlXPathContextPtr context = xmlXPathNewContext(doc);
    xmlXPathObjectPtr uses = xmlXPathEvalExpression((const xmlChar *)"/pkgmetadata/use/child::node()", context);

    if (!xmlXPathNodeSetIsEmpty(uses->nodesetval)) { // Has custom use flags
        for (int i = 0; i < uses->nodesetval->nodeNr; i++) {
            xmlNodePtr curr = uses->nodesetval->nodeTab[i];
            char *use_text = (char *)xmlNodeGetContent(curr);
            char *use_flag = (char *)xmlGetProp(curr, (const xmlChar *)"name");
            if (use_flag == NULL || use_text == NULL)
                continue;

            for (int i = 0; i < strlen(use_text); i++);
            compress_spaces(use_text);
            char *c;
            while ( (c = strchr(use_text, '\n')) != NULL ) {
                *c = ' ';
            }

            g_hash_table_insert(ALL_USE, use_flag, use_text);
        }
    }

    xmlXPathFreeContext(context);
    xmlXPathFreeObject(uses);
    xmlFreeDoc(doc);

    printf("Use flags for " ANSI_BOLD ANSI_COLOR_GREEN "%s/%s-%s" ANSI_COLOR_RESET ":\n", PACKAGE->category, PACKAGE->name, PACKAGE->version);
    printf(ANSI_BOLD " U \n" ANSI_COLOR_RESET);

    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w); // Get terminal width

    GHashTable *printed = g_hash_table_new(g_str_hash, g_str_equal); // Check if already printed but with + or -
    for (; active_iuse; active_iuse = active_iuse->next) {
        char *flag_name = (char *)active_iuse->data;
        char *c;
        if ((c = strchr(flag_name, '\n')) != NULL) // I dunno how but on my system there's \n on test
            *c = '\0';

        if (g_hash_table_lookup(printed, flag_name) != NULL) {
            continue;
        }
        g_hash_table_insert(printed, flag_name, (void *)1);

        char *descr = (char *)g_hash_table_lookup(ALL_USE, flag_name);
        if (descr == NULL)
            descr = alloc_str("<unknown>", NULL);

        char *color = ANSI_BOLD ANSI_COLOR_BLUE;

        char used = '-';
        if (flag_name[0] == '+' || flag_name[0] == '-'){
            if (flag_name[0] == '+') used = '+';
            flag_name += 1; // Move from +/-
        }

        if (!PACKAGE->installed) {
            if (used == '+') {
                color = ANSI_BOLD ANSI_COLOR_RED;
            }
        } else {
             if (g_slist_find_custom(active_use, flag_name, find_active_use) != NULL) {
                 used = '+';
             }
             if (used == '+') {
                 color = ANSI_BOLD ANSI_COLOR_RED;
             }
        }

        printf(" %c ", used);
        printf("%s" " %-*s" ANSI_COLOR_RESET ": %s\n", color, w.ws_col / 5, flag_name, descr); // ~1/5
        free(descr);
    }

    g_hash_table_destroy(printed);
    g_hash_table_destroy(ALL_USE);
    g_slist_free(active_use);
    g_slist_free(active_iuse);
}
