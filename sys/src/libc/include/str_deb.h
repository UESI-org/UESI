#ifndef _STR_DEB_H
#define _STR_DEB_H

static const char *error_strings[] = {
    "Success",
    "Operation not permitted",
    "No such file or directory",
    "No such process",
    "Interrupted system call",
    "I/O error",
    "No such device or address",
    "Argument list too long",
    "Exec format error",
    "Bad file number",
    "No child processes",
    "Try again",
    "Out of memory",
    "Permission denied",
    "Bad address",
};

static const char *signal_strings[] = {
    "Unknown signal",
    "Hangup",
    "Interrupt",
    "Quit",
    "Illegal instruction",
    "Trace/breakpoint trap",
    "Aborted",
    "Bus error",
    "Floating point exception",
    "Killed",
    "User defined signal 1",
    "Segmentation fault",
    "User defined signal 2",
    "Broken pipe",
    "Alarm clock",
    "Terminated",
};

#define NUM_ERRORS (sizeof(error_strings) / sizeof(error_strings[0]))
#define NUM_SIGNALS (sizeof(signal_strings) / sizeof(signal_strings[0]))

#endif