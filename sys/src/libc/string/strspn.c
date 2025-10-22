#include "../include/string.h"
#include <stdint.h>

size_t strspn(const char *s, const char *accept) {
    size_t count = 0;
    const char *a;
    
    for (; *s; s++, count++) {
        for (a = accept; *a && *a != *s; a++);
        if (!*a)
            break;
    }
    
    return count;
}