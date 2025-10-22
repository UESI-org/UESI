#include "../include/string.h"
#include <stdint.h>

char *strtok_r(char *str, const char *delim, char **saveptr) {
    char *token;
    
    if (str == NULL)
        str = *saveptr;
    
    if (str == NULL)
        return NULL;
    
    str += strspn(str, delim);
    
    if (*str == '\0') {
        *saveptr = NULL;
        return NULL;
    }
    
    token = str;
    str = strpbrk(token, delim);
    
    if (str == NULL) {
        *saveptr = NULL;
    } else {
        *str = '\0';
        *saveptr = str + 1;
    }
    
    return token;
}