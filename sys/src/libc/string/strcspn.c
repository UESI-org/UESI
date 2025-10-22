#include "../include/string.h"
#include <stdint.h>

size_t strcspn(const char *s, const char *reject) {
    size_t count = 0;
    const char *r;
    
    for (; *s; s++, count++) {
        for (r = reject; *r && *r != *s; r++);
        if (*r)
            break;
    }
    
    return count;
}