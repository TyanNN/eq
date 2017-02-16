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

#include "sds.h"

extern const char *PORTAGE_DB_DIR;
extern const char *PORTAGE_EBUILDS_DIR;
extern const char *LAYMAN_EBUILDS_DIR;

GHashTable *ALL_USE;

void parse_desc(sds path) {
    FILE *f = fopen(path, "r");

    if (f == NULL) {
        perror("I/O ERROR: Cant open desc file");
        return;
    }

    char *chr = read_file_content(f);
    sds st = sdsnew(chr);
    fclose(f);

    int count;

    sds *tokens = sdssplitlen(st, sdslen(st), "\n", 1, &count);

    for (int i = 0; i < count; i++) {
        if (tokens[i][0] != '#' && sdslen(tokens[i]) > 2) {
            int cs;
            sds *this_line = sdssplitlen(tokens[i], sdslen(tokens[i]), " - ", 3, &cs);

            char *use_flag_s = this_line[0];
            char *use_text_s = this_line[1];

            sds use_flag = sdsnew(use_flag_s);
            sds use_text = sdsnew(use_text_s);

            if (strstr(path, "use.desc") == NULL) {
                sds flname = sdsnew(basename(path));
                sdsrange(flname, 0, -(5 + 1)); // .desc
                use_flag = sdscatprintf(flname, "_%s", use_flag);
            }
            g_hash_table_insert(ALL_USE, use_flag, use_text);

            sdsfreesplitres(this_line, cs);
        }
    }
    sdsfreesplitres(tokens, count);

    sdsfree(st);
    free(chr);
}

int find_active_use(const void *el, const void *tofind) {
    if (strcmp((char *)el, (char *)tofind) == 0) {
        return 0;
    } else {
        return 1;
    }
}

void destr_keys(void *k) {
    sdsfree((sds)k);
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
            parse_desc(alloc_str("/usr/portage/profiles/desc/", el->d_name, NULL));
        }
    }
    closedir(desc_dir);

    sds dir;
    if (strcmp(PACKAGE->repository, "gentoo") == 0) {
        dir = alloc_str(PORTAGE_EBUILDS_DIR, NULL); // From portage tree
    } else {
        dir = alloc_str(LAYMAN_EBUILDS_DIR, "/", PACKAGE->repository, NULL); // From overlay
    }

    // Per-package use flags descriptions
    sds pth = alloc_str(dir, "/", PACKAGE->category, "/", PACKAGE->name, "/metadata.xml", NULL);
    xmlDocPtr doc;

    if (access(pth, F_OK) != -1) {
        doc = xmlParseFile(pth);

        xmlXPathContextPtr context = xmlXPathNewContext(doc);
        xmlXPathObjectPtr uses = xmlXPathEvalExpression((const xmlChar *)"/pkgmetadata/use/child::node()", context);

        if (!xmlXPathNodeSetIsEmpty(uses->nodesetval)) { // Has custom use flags
            for (int i = 0; i < uses->nodesetval->nodeNr; i++) {
                xmlNodePtr curr = uses->nodesetval->nodeTab[i];

                xmlChar *sut = xmlNodeListGetString(doc, curr->xmlChildrenNode, true);
                sds use_text = sdsnew((char *)sut);
                xmlChar *suf = xmlGetProp(curr, (xmlChar *)"name");
                sds use_flag = sdsnew((char *)suf);

                if (use_flag == NULL || use_text == NULL)
                    continue;

                compress_spaces(use_text);
                sdstrim(use_text, "\n\t");

                g_hash_table_insert(ALL_USE, use_flag, use_text);

                free(sut);
                free(suf);
            }
        }

        xmlXPathFreeContext(context);
        xmlXPathFreeObject(uses);
    }
    xmlFreeDoc(doc);

    sdsfree(pth);
    sdsfree(dir);

    if (PACKAGE->installed) {
        sds active_use_pth = alloc_str(PORTAGE_DB_DIR, "/", PACKAGE->category, "/", PACKAGE->name, "-", PACKAGE->version, "/USE", NULL);
        sds active_iuse_pth = alloc_str(PORTAGE_DB_DIR, "/", PACKAGE->category, "/", PACKAGE->name, "-", PACKAGE->version, "/IUSE", NULL);

        FILE *active_use_file = fopen(active_use_pth, "r");
        FILE *active_iuse_file = fopen(active_iuse_pth, "r");

        char *aus = read_file_content(active_use_file);
        char *ais = read_file_content(active_iuse_file);

        sds active_use_s = sdsnew(aus);
        sds active_iuse_s = sdsnew(ais);

        fclose(active_use_file);
        fclose(active_iuse_file);

        sdsfree(active_use_pth);
        sdsfree(active_iuse_pth);

        sds *tokens;
        int count;

        tokens = sdssplitlen(active_use_s, sdslen(active_use_s), " ", 1, &count);
        for (int i = 0; i < count; i++) {
            if (sdslen(tokens[i]) > 1)
                active_use = g_slist_append(active_use, sdsdup(tokens[i]));
        }
        sdsfreesplitres(tokens, count);

        sds *itokens;
        int icount;

        itokens = sdssplitlen(active_iuse_s, sdslen(active_iuse_s), " ", 1, &icount);
        for (int i = 0; i < icount; i++) {
            if (sdslen(itokens[i]) > 1)
                active_iuse = g_slist_append(active_iuse, sdsdup(itokens[i]));
        }
        sdsfreesplitres(itokens, icount);

        sdsfree(active_use_s);
        sdsfree(active_iuse_s);

        free(aus);
        free(ais);
    } else {
        sds eb_path = sdsempty();
        if (strcmp(PACKAGE->repository, "gentoo") == 0) {
            eb_path = sdscatprintf(eb_path, "%s", alloc_str(PORTAGE_EBUILDS_DIR,
                        "/",
                        PACKAGE->category,
                        "/", PACKAGE->name, "/", PACKAGE->name, "-", PACKAGE->version, ".ebuild", NULL));
        }
        FILE *eb_f = fopen(eb_path, "r");
        sds eb_s = sdsnew(read_file_content(eb_f));
        fclose(eb_f);
        sdsfree(eb_path);

        char *token = strstr(eb_s, "IUSE=");
        while (token != NULL) {
            unsigned long int cap = 5;
            unsigned int n = 0;

            sds iuses = sdsempty();
            for (int i = 7; i < strlen(token); i++) { // IUSE="
               if (token[i] != '\"') {
                   iuses = sdscatprintf(iuses, "%c", token[i]);
               }
            }

            sdstrim(iuses, "\n\t");
            compress_spaces(iuses);

            char *chr;
            for(int i = 0; i < strlen(iuses); i++) {
                if (isalnum(iuses[i]) || iuses[i] == '+' || iuses[i] == '-') {
                    chr = &iuses[i];
                    break;
                }
            }

            char *tt = strtok(iuses, " ");
            while (tt != NULL) {
                active_iuse = g_slist_append(active_iuse, tt);
                tt = strtok(NULL, " ");
            }

            token = strstr(token + strlen(iuses), "IUSE");
            sdsfree(iuses);
        }
        sdsfree(eb_s);
    }

    printf("Use flags for " ANSI_BOLD ANSI_COLOR_GREEN "%s/%s-%s" ANSI_COLOR_RESET ":\n", PACKAGE->category, PACKAGE->name, PACKAGE->version);
    printf(ANSI_BOLD " U \n" ANSI_COLOR_RESET);

    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w); // Get terminal width

    GHashTable *printed = g_hash_table_new(g_str_hash, g_str_equal); // Check if already printed but with + or -
    for (; active_iuse; active_iuse = active_iuse->next) {
        sds flag_name = (sds)active_iuse->data;
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
            sdsrange(flag_name, 1, -1); // Move from +/-
        }

        sds descr = g_hash_table_lookup(ALL_USE, flag_name);
        if (descr == NULL)
            descr = alloc_str("<unknown>", NULL);

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

        sdsfree(descr);
    }

    g_hash_table_destroy(printed);
    g_hash_table_destroy(ALL_USE);
    g_slist_free(active_use);
    g_slist_free(active_iuse);
}
