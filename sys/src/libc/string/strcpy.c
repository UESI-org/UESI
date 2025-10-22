#include "../include/string.h"
#include <stdint.h>

char *strcpy(char *dest, const char *src) {
    char *d = dest;
    
    while ((*d++ = *src++));
    
    return dest;
}