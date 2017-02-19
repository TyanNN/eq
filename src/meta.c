#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <glib.h>

#include <libxml/parser.h>
#include <libxml/xpath.h>

#include "shared.h"
#include "ebuild_parser.h"

extern const char *PORTAGE_EBUILDS_DIR;
extern const char *LAYMAN_EBUILDS_DIR;
extern package_info_t *PACKAGE;

void meta() {
    char *maind = !strcmp(PACKAGE->repository, "gentoo") ?
        g_strconcat(PORTAGE_EBUILDS_DIR, "/", PACKAGE->category, "/", PACKAGE->name, NULL) :
        g_strconcat(LAYMAN_EBUILDS_DIR, "/", PACKAGE->repository, "/", PACKAGE->category, "/", PACKAGE->name, NULL);
    char *pth = g_strconcat(maind, "/metadata.xml", NULL);

    char *maint_email = NULL, *maint_name = NULL;
    xmlDocPtr doc = NULL;
    if (access(pth, F_OK) != -1) {
        doc = xmlParseFile(pth);

        xmlXPathContextPtr context = xmlXPathNewContext(doc);
        xmlXPathObjectPtr maint = xmlXPathEvalExpression((const xmlChar *)"/pkgmetadata/maintainer/*[self::name or self::email]", context);

        if (!xmlXPathNodeSetIsEmpty(maint->nodesetval)) {
            for (int i = 0; i < maint->nodesetval->nodeNr; i++) {
                xmlNodePtr curr = maint->nodesetval->nodeTab[i];

                if (strcmp((char *)curr->name, "name") == 0) {
                    maint_name = (char *)xmlNodeGetContent(curr);
                } else if (strcmp((char *)curr->name, "email") == 0) {
                    maint_email = (char *)xmlNodeGetContent(curr);
                }
            }
        }
        xmlXPathFreeContext(context);
        xmlXPathFreeObject(maint);
    }
    if (doc != NULL) {
        xmlFreeDoc(doc);
    }

    printf(ANSI_COLOR_GREEN ANSI_BOLD "%s/%s" ANSI_COLOR_RESET " [" ANSI_BOLD ANSI_COLOR_CYAN "%s" ANSI_COLOR_RESET "]\n",
            PACKAGE->category, PACKAGE->name, PACKAGE->repository);
    printf(ANSI_BOLD "%-20s " ANSI_COLOR_RESET "%s <%s>\n", "Maintainer:", maint_name, maint_email);
    printf(ANSI_BOLD "%-20s " ANSI_COLOR_RESET "%s\n", "Location:", maind);

    GDir *dr = g_dir_open(maind, 0, NULL);

    char *fname;
    char *homepage = NULL;
    char *license = NULL;

    while ((fname = (char *)g_dir_read_name(dr)) != NULL) {
        if (g_str_has_suffix(fname, ".ebuild")) {
            char *fullp = g_strconcat(maind, "/", fname, NULL);
            FILE *f = fopen(fullp, "r");
            if (homepage == NULL) {
                homepage = parse_homepage(f);
                printf(ANSI_BOLD "%-20s " ANSI_COLOR_RESET "%s\n", "Homepage:", homepage);
                free(homepage);
            }
            if (license == NULL) {
                license = parse_license(f);
                printf(ANSI_BOLD "%-20s " ANSI_COLOR_RESET "%s\n", "License:", license);
                free(license);
            }

            char *keywords = parse_keywords(f);
            char *name = g_strndup(fname + strlen(PACKAGE->name) + 1, strlen(fname + strlen(PACKAGE->name) + 1) - 7); // .ebuild

            printf(ANSI_BOLD "%s %-10s" ANSI_COLOR_RESET, "Keywords", name);

            if (keywords == NULL) {
                printf("\n");
            } else {
                printf("%s\n", keywords);
            }

            fclose(f);
            free(name);
            free(fullp);
        }
    }

    g_dir_close(dr);

    free(pth);
    free(maind);
}
