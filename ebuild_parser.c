#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <ctype.h>

#include <glib.h>

typedef enum {
    IUSE_NOT_STARTED,
    IUSE_PARSING,
    IUSE_ENDED
} iuse_parse_state;

static bool isempty(const char *s) {
    while (*s != '\0') {
        if (!isspace(*s)) {
            return 0;
        }
        s++;
    }
    return 1;
}

char *parse_homepage(FILE *f) {
    size_t len;
    ssize_t llen;
    char *line;
    while ((llen = getline(&line, &len, f)) != -1) {
        if (g_str_has_prefix(line, "HOMEPAGE")) {
            return g_strndup(line + 10, strlen(line + 10) - 2);
        }
    }
    return NULL;
}

char *parse_license(FILE *f) {
    size_t len;
    ssize_t llen;
    char *line;
    while ((llen = getline(&line, &len, f)) != -1) {
        if (g_str_has_prefix(line, "LICENSE")) {
            return g_strndup(line + 9, strlen(line + 9) - 2);
        }
    }
    return NULL;
}

char *parse_keywords(FILE *f) {
    size_t len;
    ssize_t llen;
    char *line;
    char *res = calloc(1, sizeof(char));
    while ((llen = getline(&line, &len, f)) != -1) {
        if (g_str_has_prefix(line, "KEYWORDS")) {
            return g_strndup(line + 10, strlen(line + 10) - 2);
        }
    }
    return NULL;
}

int parse_iuse(FILE *f, GPtrArray *arr) {
    if (f == NULL) {
        perror("Error opening file");
        return -1;
    }
    char *line;
    size_t len;
    ssize_t llen;
    iuse_parse_state iuse_ended = IUSE_NOT_STARTED;
    while ((llen = getline(&line, &len, f)) != -1) {
        char *iuse_start;
        if ((iuse_start = strstr(line, "IUSE")) != NULL || iuse_ended == IUSE_PARSING) {
            unsigned int move_chars = 0;
            if (iuse_ended == IUSE_NOT_STARTED || iuse_ended == IUSE_ENDED) {
                move_chars = 5; // IUSE=
                if ((*(iuse_start + 3 + 1)) == '+' || (*(iuse_start + 3 + 1)) == '-')
                    move_chars++;
                assert((iuse_start + move_chars)[0] == '\"');
            }

            char *flags = calloc((llen - move_chars) + 1, sizeof(char));
            if (iuse_ended == IUSE_NOT_STARTED || iuse_ended == IUSE_ENDED) {
                memcpy(flags, iuse_start + move_chars + 1, sizeof(char) * ((llen - move_chars) - 2)); // no newline and first "
            } else {
                memcpy(flags, line, sizeof(char) * (llen - 1));
            }

            char *token;
            while ((token = strsep(&flags, " "))) {
                if (isempty(token) || token[0] == '$') { // Skip varibles, TODO
                    continue;
                }
                if (token[strlen(token) - 1] == '\"') {
                    token[strlen(token) - 1] = '\0';
                    iuse_ended = IUSE_ENDED;
                } else {
                    iuse_ended = IUSE_PARSING;
                }

                g_ptr_array_insert(arr, -1, g_strstrip(token));
            }

            free(flags);
        }
    }
    free(line);

    return 0;
}
