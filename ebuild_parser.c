#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <ctype.h>

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

int parse_iuse(FILE *f, char ***arr, size_t *el_count) {
    char *line;
    size_t len;

    *el_count = 0;

    size_t cap = 3;

    if (el_count == NULL)
        el_count = 0;

    *arr = malloc(sizeof(char *) * cap);

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

            char *flags = malloc(sizeof(char) * ((llen - move_chars) + 1));
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

                if (++(*el_count) > cap) {
                    cap *= 2;
                    char **tmp = realloc(*arr, sizeof(char *) * cap);
                    if (tmp == NULL) {
                        free(tmp);
                        perror("Failed to allocate memory for iuse");
                        return -1;
                    }

                    *arr = tmp;
                    (*arr)[*el_count - 1] = token;
                } else {
                    (*arr)[*el_count - 1] = token;
                }
            }

            free(flags);
        }
    }
    free(line);

    return 0;
}
