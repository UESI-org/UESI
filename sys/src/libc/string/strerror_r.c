#include "../include/string.h"
#include "../include/str_deb.h"
#include <stdint.h>

int strerror_r(int errnum, char *buf, size_t buflen) {
    if (buflen == 0)
        return -1;
    
    const char *msg;
    if (errnum >= 0 && errnum < (int)NUM_ERRORS)
        msg = error_strings[errnum];
    else
        msg = "Unknown error";
    
    size_t len = strlen(msg);
    if (len >= buflen) {
        memcpy(buf, msg, buflen - 1);
        buf[buflen - 1] = '\0';
        return -1;
    }
    
    memcpy(buf, msg, len + 1);
    return 0;
}
