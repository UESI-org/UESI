#include "../include/string.h"
#include <stdint.h>

char *strpbrk(const char *s, const char *accept) {
    const char *a;
    
    for (; *s; s++) {
        for (a = accept; *a; a++) {
            if (*s == *a)
                return (char *)s;
        }
    }
    
    return NULL;
}