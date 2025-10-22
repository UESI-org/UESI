#include "../include/string.h"
#include <stdint.h>

char *strtok(char *str, const char *delim) {
    static char *saveptr = NULL;
    return strtok_r(str, delim, &saveptr);
}