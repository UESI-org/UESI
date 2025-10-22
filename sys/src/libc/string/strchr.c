#include "../include/string.h"
#include <stdint.h>

char *strchr(const char *s, int c) {
    char ch = c;
    
    while (*s) {
        if (*s == ch)
            return (char *)s;
        s++;
    }
    
    if (ch == '\0')
        return (char *)s;
    
    return NULL;
}