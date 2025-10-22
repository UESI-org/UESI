#ifndef _STRING_H
#define _STRING_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void *memchr(const void *s, int c, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
void *memcpy(void *dest, const void *src, size_t n);
void *memmove(void *dest, const void *src, size_t n);
void *memset(void *s, int c, size_t n);

char *strchr(const char *s, int c);
char *strrchr(const char *s, int c);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
int strcoll(const char *s1, const char *s2);
size_t strlen(const char *s);
size_t strspn(const char *s, const char *accept);
size_t strcspn(const char *s, const char *reject);
char *strpbrk(const char *s, const char *accept);
char *strstr(const char *haystack, const char *needle);

char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, size_t n);
char *strcat(char *dest, const char *src);
char *strncat(char *dest, const char *src, size_t n);
char *strdup(const char *s);
char *strndup(const char *s, size_t n);
size_t strxfrm(char *dest, const char *src, size_t n);

char *strtok(char *str, const char *delim);
char *strtok_r(char *str, const char *delim, char **saveptr);

char *strerror(int errnum);
int strerror_r(int errnum, char *buf, size_t buflen);

char *strsignal(int sig);

#ifdef __cplusplus
}
#endif

#endif