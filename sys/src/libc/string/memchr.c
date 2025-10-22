#include "../include/string.h"
#include <stdint.h>

void *memchr(const void *s, int c, size_t n) {
    const unsigned char *p = s;
    unsigned char uc = c;
    
    for (size_t i = 0; i < n; i++) {
        if (p[i] == uc)
            return (void *)(p + i);
    }
    return NULL;
}