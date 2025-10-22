#include "../include/string.h"
#include "../include/str_deb.h"
#include <stdint.h>

char *strerror(int errnum) {
    static char buf[32];
    
    if (errnum >= 0 && errnum < (int)NUM_ERRORS)
        return (char *)error_strings[errnum];
    
    char *p = buf + sizeof(buf) - 1;
    *p = '\0';
    
    int n = errnum < 0 ? -errnum : errnum;
    do {
        *--p = '0' + (n % 10);
        n /= 10;
    } while (n > 0 && p > buf);
    
    if (errnum < 0 && p > buf)
        *--p = '-';
    
    const char *prefix = "Unknown error ";
    while (*prefix && p > buf)
        *--p = *prefix++;
    
    return p;
}