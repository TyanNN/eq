#ifndef H_EQU_SHARED
#define H_EQU_SHARED

#define ANSI_COLOR_RED      "\x1b[31m"
#define ANSI_COLOR_GREEN    "\x1b[32m"
#define ANSI_COLOR_YELLOW   "\x1b[33m"
#define ANSI_COLOR_BLUE     "\x1b[34m"
#define ANSI_COLOR_MAGENTA  "\x1b[35m"
#define ANSI_COLOR_CYAN     "\x1b[36m"
#define ANSI_COLOR_RESET    "\x1b[0m"
#define ANSI_BOLD           "\033[1m"

#ifdef PORTAGE_DIRS
const char* PORTAGE_DB_DIR = "/var/db/pkg"; // no / is important in these two
const char* PORTAGE_EBUILDS_DIR = "/usr/portage";
#endif

typedef struct {
    char *name;
    char *version;
    char *category;
    char *repository;
} package_info_t;

package_info_t *PACKAGE;

#endif
