#ifndef _STDLIB_H_
#define _STDLIB_H_

#include <sys/cdefs.h>
#include <sys/_null.h>
#include <sys/_types.h>

#include <stdint.h>

#ifndef _SIZE_T_DEFINED_
#define _SIZE_T_DEFINED_
typedef __size_t size_t;
#endif

#ifndef _WCHAR_T_DEFINED_
#define _WCHAR_T_DEFINED_
typedef __wchar_t wchar_t;
#endif

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

#define RAND_MAX 0x7fffffff

#define MB_CUR_MAX 1

__BEGIN_DECLS

void *malloc(size_t size);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);
void *reallocarray(void *ptr, size_t nmemb, size_t size);
void free(void *ptr);

#if __POSIX_VISIBLE >= 200112 || __ISO_C_VISIBLE >= 2011
void *aligned_alloc(size_t alignment, size_t size);
#endif

#if __POSIX_VISIBLE >= 200112
int posix_memalign(void **memptr, size_t alignment, size_t size);
#endif

__dead void abort(void);
int atexit(void (*function)(void));
__dead void exit(int status);
__dead void _Exit(int status);

char *getenv(const char *name);
#if __POSIX_VISIBLE >= 200112
int setenv(const char *name, const char *value, int overwrite);
int unsetenv(const char *name);
int putenv(char *string);
#endif

int atoi(const char *nptr);
long atol(const char *nptr);
long long atoll(const char *nptr);

long strtol(const char *__restrict nptr, char **__restrict endptr, int base);
unsigned long strtoul(const char *__restrict nptr, char **__restrict endptr, int base);
long long strtoll(const char *__restrict nptr, char **__restrict endptr, int base);
unsigned long long strtoull(const char *__restrict nptr, char **__restrict endptr, int base);

double atof(const char *nptr);
double strtod(const char *__restrict nptr, char **__restrict endptr);
float strtof(const char *__restrict nptr, char **__restrict endptr);
long double strtold(const char *__restrict nptr, char **__restrict endptr);

int abs(int j);
long labs(long j);
long long llabs(long long j);

typedef struct {
	int quot;
	int rem;
} div_t;

typedef struct {
	long quot;
	long rem;
} ldiv_t;

typedef struct {
	long long quot;
	long long rem;
} lldiv_t;

div_t div(int numer, int denom);
ldiv_t ldiv(long numer, long denom);
lldiv_t lldiv(long long numer, long long denom);

int rand(void);
void srand(unsigned int seed);

#if __POSIX_VISIBLE >= 200112
int rand_r(unsigned int *seedp);
#endif

void *bsearch(const void *key, const void *base, size_t nmemb, size_t size,
              int (*compar)(const void *, const void *));
void qsort(void *base, size_t nmemb, size_t size,
           int (*compar)(const void *, const void *));

int mblen(const char *s, size_t n);
int mbtowc(wchar_t *__restrict pwc, const char *__restrict s, size_t n);
int wctomb(char *s, wchar_t wchar);
size_t mbstowcs(wchar_t *__restrict pwcs, const char *__restrict s, size_t n);
size_t wcstombs(char *__restrict s, const wchar_t *__restrict pwcs, size_t n);

#if __POSIX_VISIBLE >= 199506 || __XPG_VISIBLE
int system(const char *command);
#endif

#if __POSIX_VISIBLE >= 200112
char *realpath(const char *__restrict path, char *__restrict resolved_path);
#endif

#if __BSD_VISIBLE
const char *getprogname(void);
void setprogname(const char *name);

uint32_t arc4random(void);
void arc4random_buf(void *buf, size_t nbytes);
uint32_t arc4random_uniform(uint32_t upper_bound);

void *recallocarray(void *ptr, size_t oldnmemb, size_t nmemb, size_t size);
void freezero(void *ptr, size_t size);
#endif

__END_DECLS

#endif