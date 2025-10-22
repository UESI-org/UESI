#include "../include/string.h"
#include <stdint.h>

char *strrchr(const char *s, int c) {
    char ch = c;
    const char *last = NULL;
    
    while (*s) {
        if (*s == ch)
            last = s;
        s++;
    }
    
    if (ch == '\0')
        return (char *)s;
    
    return (char *)last;
}