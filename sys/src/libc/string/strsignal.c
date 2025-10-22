#include "../include/string.h"
#include "../include/str_deb.h"
#include <stdint.h>

char *strsignal(int sig) {
    if (sig >= 0 && sig < (int)NUM_SIGNALS)
        return (char *)signal_strings[sig];
    
    return "Unknown signal";
}