#ifndef PUTS_H
#define PUTS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int puts(const char *str);

int putstr(const char *str);

int putchar(int c);

int putchar_n(int c, size_t n);

#ifdef __cplusplus
}
#endif

#endif