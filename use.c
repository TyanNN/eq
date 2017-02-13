#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <libgen.h>

#include <glib.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>

#include "utils.h"
#include "shared.h"

extern const char* PORTAGE_DB_DIR;
extern const char* PORTAGE_EBUILDS_DIR;

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

void parse_use() {
    ALL_USE = g_hash_table_new(g_str_hash, g_str_equal);

    GSList *active_use = NULL;
    GSList *active_iuse = NULL;

    /* /usr/portage/profiles/use.desc
     * /usr/portage/profiles/desc */

    //Global use flags
    parse_desc("/usr/portage/profiles/use.desc");

    DIR *desc_dir = opendir("/usr/portage/profiles/desc");
    struct dirent *el;

    while ((el = readdir(desc_dir)) != NULL) {
        char *fullpath = alloc_str("/usr/portage/profiles/desc/", el->d_name, NULL);
        parse_desc(fullpath);
        free(fullpath);
    }

    if (strcmp(PACKAGE->repository, "gentoo") == 0) {

        // Per-package use flags
        char *pth = alloc_str(PORTAGE_EBUILDS_DIR, "/", PACKAGE->category, "/", PACKAGE->name, "/metadata.xml", NULL);

        xmlDocPtr doc = xmlParseFile(pth);

        free(pth);

        if (doc == NULL) {
            printf("No metadata.xml");
            return;
        }

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

        xmlXPathContextPtr context = xmlXPathNewContext(doc);
        xmlXPathObjectPtr uses = xmlXPathEvalExpression((const xmlChar *)"/pkgmetadata/use/child::node()", context);

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

        xmlXPathFreeContext(context);
        xmlXPathFreeObject(uses);
        xmlFreeDoc(doc);
    }

    GHashTable *printed = g_hash_table_new(g_str_hash, g_str_equal); // Check if already printed but with + or -
    for (; active_iuse; active_iuse = active_iuse->next) {
        char *flag_name = (char *)active_iuse->data;
        if (flag_name[0] == '+' || flag_name[0] == '-')
            flag_name = flag_name + 1; // Move from +/-
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

        GSList *data;
        if ( (data = g_slist_find_custom(active_use, flag_name, find_active_use)) != NULL) {
            printf(ANSI_BOLD ANSI_COLOR_RED "%2c %-23s" ANSI_COLOR_RESET ": %s", ' ', flag_name, descr);
        } else {
            printf(ANSI_BOLD ANSI_COLOR_BLUE "%2c %-23s" ANSI_COLOR_RESET ": %s", ' ', flag_name, descr);
        }
        printf("\n");
        free(descr);
    }

    g_hash_table_destroy(printed);
    g_hash_table_destroy(ALL_USE);
    g_slist_free(active_use);
    g_slist_free(active_iuse);
}
