#include "../include/string.h"
#include <stdint.h>

char *strncat(char *dest, const char *src, size_t n) {
    char *d = dest;
    size_t i;
    
    while (*d)
        d++;
    
    for (i = 0; i < n && src[i]; i++)
        d[i] = src[i];
    
    d[i] = '\0';
    
    return dest;
}