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

#include "ebuild_parser.h"

extern const char *PORTAGE_DB_DIR;
extern const char *PORTAGE_EBUILDS_DIR;
extern const char *LAYMAN_EBUILDS_DIR;

GHashTable *ALL_USE;

void parse_desc(char *path) {
    char *st;
    g_file_get_contents(path, &st, NULL, NULL);

    char **tokens = g_strsplit(st, "\n", 0);

    for (int i = 0; i < g_strv_length(tokens); i++) {
        if (!g_str_has_prefix(tokens[i], "#") && strlen(tokens[i]) > 2) { // Ignore comments
            char **this_line = g_strsplit(tokens[i], "-", 0);
            char *use_flag = g_strndup(this_line[0], strlen(this_line[0]) - 1);
            char *use_text = g_strstrip(g_strjoinv("-", this_line + 1));

            if (!g_str_has_suffix(path, "use.desc")) {
                char *filename = g_strndup(basename(path), strlen(basename(path)) - 5); // .desc
                use_flag = g_strconcat(filename, "_", use_flag, NULL);
                free(filename);
            }
            g_hash_table_insert(ALL_USE, use_flag, use_text);
            g_strfreev(this_line);
        }
    }
    free(st);
    g_strfreev(tokens);
}

int find_active_use(const void *el, const void *tofind) {
    if (strcmp((char *)el, (char *)tofind) == 0) {
        return 0;
    } else {
        return 1;
    }
}

void destr_keys(void *k) {
    free((char *)k);
}

// TODO: do something with use expand
void parse_use() {
    ALL_USE = g_hash_table_new_full(g_str_hash, g_str_equal, destr_keys, NULL); // All keys are malloc'ed so we need to delete them

    GSList *active_use = NULL;
    GSList *active_iuse = NULL;

    //Global use flags descriptions
    parse_desc("/usr/portage/profiles/use.desc");

    // Other descriptions
    DIR *desc_dir = opendir("/usr/portage/profiles/desc");
    struct dirent *el;
    while ((el = readdir(desc_dir)) != NULL) {
        if (strcmp(el->d_name, ".") != 0 && strcmp(el->d_name, "..") != 0) {
            char *tmpp = g_strconcat("/usr/portage/profiles/desc/", el->d_name, NULL);
            parse_desc(tmpp);
            free(tmpp);
        }
    }
    closedir(desc_dir);

    char *dir;
    if (strcmp(PACKAGE->repository, "gentoo") == 0) {
        dir = g_strconcat(PORTAGE_EBUILDS_DIR, NULL); // From portage tree
    } else {
        dir = g_strconcat(LAYMAN_EBUILDS_DIR, "/", PACKAGE->repository, NULL); // From overlay
    }

    // Per-package use flags descriptions
    char *pth = g_strconcat(dir, "/", PACKAGE->category, "/", PACKAGE->name, "/metadata.xml", NULL);
    xmlDocPtr doc = NULL;

    if (access(pth, F_OK) != -1) {
        doc = xmlParseFile(pth);

        xmlXPathContextPtr context = xmlXPathNewContext(doc);
        xmlXPathObjectPtr uses = xmlXPathEvalExpression((const xmlChar *)"/pkgmetadata/use/child::node()", context);

        if (!xmlXPathNodeSetIsEmpty(uses->nodesetval)) { // Has custom use flags
            for (int i = 0; i < uses->nodesetval->nodeNr; i++) {
                xmlNodePtr curr = uses->nodesetval->nodeTab[i];

                xmlChar *sut = xmlNodeListGetString(doc, curr->xmlChildrenNode, true);
                char *use_text = g_strdup((char *)sut);
                xmlChar *suf = xmlGetProp(curr, (xmlChar *)"name");
                char *use_flag = g_strdup((char *)suf);

                if (use_flag == NULL || use_text == NULL)
                    continue;

                compress_spaces(use_text);
                use_text = g_strstrip(use_text);

                g_hash_table_insert(ALL_USE, use_flag, use_text);

                free(sut);
                free(suf);
            }
        }

        xmlXPathFreeContext(context);
        xmlXPathFreeObject(uses);
    }
    if (doc != NULL)
        xmlFreeDoc(doc);

    free(pth);
    free(dir);

    if (PACKAGE->installed) {
        char *active_use_pth = g_strconcat(PORTAGE_DB_DIR, "/", PACKAGE->category, "/", PACKAGE->name, "-", PACKAGE->version, "/USE", NULL);
        char *active_iuse_pth = g_strconcat(PORTAGE_DB_DIR, "/", PACKAGE->category, "/", PACKAGE->name, "-", PACKAGE->version, "/IUSE", NULL);

        char *active_use_s, *active_iuse_s;
        g_file_get_contents(active_use_pth, &active_use_s, NULL, NULL);
        g_file_get_contents(active_iuse_pth, &active_iuse_s, NULL, NULL);

        free(active_use_pth);
        free(active_iuse_pth);

        char **tokens = g_strsplit(active_use_s, " ", 0);
        for (int i = 0; i < g_strv_length(tokens); i++) {
            if (strlen(tokens[i]) > 1) {
                active_use = g_slist_append(active_use, g_strdup(tokens[i]));
            }
        }
        g_strfreev(tokens);

        char **itokens = g_strsplit(active_iuse_s, " ", 0);
        for (int i = 0; i < g_strv_length(itokens); i++) {
            if (strlen(itokens[i]) > 1) {
                active_iuse = g_slist_append(active_iuse, g_strdup(itokens[i]));
            }
        }
        g_strfreev(itokens);

        free(active_use_s);
        free(active_iuse_s);
    } else {
        char *eb_path;
        if (strcmp(PACKAGE->repository, "gentoo") == 0) {
            eb_path = g_strconcat(PORTAGE_EBUILDS_DIR, "/", PACKAGE->category, "/", PACKAGE->name, "/", PACKAGE->name, "-", PACKAGE->version, ".ebuild", NULL);
        }

        FILE *eb_f = fopen(eb_path, "r");
        GPtrArray *flags = g_ptr_array_new();
        parse_iuse(eb_f, flags);
        fclose(eb_f);
        free(eb_path);

        for (int i = 0; i < flags->len; i++) {
            char *tmp = g_ptr_array_index(flags, i);
            tmp = g_strchomp(tmp);
            active_iuse = g_slist_append(active_iuse, tmp);
        }
        g_ptr_array_free(flags, true);
    }

    printf("Use flags for " ANSI_BOLD ANSI_COLOR_GREEN "%s/%s-%s" ANSI_COLOR_RESET ":\n", PACKAGE->category, PACKAGE->name, PACKAGE->version);
    printf(ANSI_BOLD " U \n" ANSI_COLOR_RESET);

    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w); // Get terminal width

    GHashTable *printed = g_hash_table_new(g_str_hash, g_str_equal); // Check if already printed but with + or -
    for (; active_iuse; active_iuse = active_iuse->next) {
        char *flag_name = active_iuse->data;
        char *c;
        if ((c = strchr(flag_name, '\n')) != NULL) // I dunno how but on my system there's \n on test
            *c = '\0';

        if (g_hash_table_lookup(printed, flag_name) != NULL) {
            continue;
        }
        g_hash_table_insert(printed, flag_name, (void *)1);

        char *color = ANSI_BOLD ANSI_COLOR_BLUE;

        char used = '-';
        if (flag_name[0] == '+' || flag_name[0] == '-'){
            if (flag_name[0] == '+') used = '+';
            flag_name = flag_name + 1; // Move from +/-
        }

        char *descr = g_hash_table_lookup(ALL_USE, flag_name);
        if (descr == NULL)
            descr = "<unknown>";

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
    }

    g_hash_table_destroy(printed);
    g_hash_table_destroy(ALL_USE);
    g_slist_free(active_use);
    g_slist_free(active_iuse);
}
