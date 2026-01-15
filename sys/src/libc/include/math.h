#ifndef _MATH_H_
#define _MATH_H_

#include <sys/cdefs.h>

/*
 * ANSI/POSIX
 */
extern const union __infinity_un {
	unsigned char __uc[8];
	double __ud;
} __infinity;

extern const union __nan_un {
	unsigned char __uc[8];
	double __ud;
} __nan;

#if __GNUC_PREREQ__(3, 3)
#define __MATH_CONST __attribute__((__const__))
#else
#define __MATH_CONST
#endif

#if __GNUC_PREREQ__(3, 0) && !defined(__cplusplus)
#define __MATH_INLINE __attribute__((__always_inline__)) static __inline
#else
#define __MATH_INLINE static __inline
#endif

#define HUGE_VAL __builtin_huge_val()

#if __ISO_C_VISIBLE >= 1999
#define FP_INFINITE 0x01
#define FP_NAN 0x02
#define FP_NORMAL 0x04
#define FP_SUBNORMAL 0x08
#define FP_ZERO 0x10

#define FP_ILOGB0 (-2147483647 - 1)
#define FP_ILOGBNAN (-2147483647 - 1)

#define HUGE_VALF __builtin_huge_valf()
#define HUGE_VALL __builtin_huge_vall()
#define INFINITY __builtin_inff()
#define NAN __builtin_nanf("")

#define MATH_ERRNO 1
#define MATH_ERREXCEPT 2
#define math_errhandling MATH_ERRNO

#define FP_FAST_FMA 1
#define FP_FAST_FMAF 1
#define FP_FAST_FMAL 1

typedef double double_t;
typedef float float_t;
#endif /* __ISO_C_VISIBLE >= 1999 */

__BEGIN_DECLS

/*
 * ANSI C89/C90
 */
double acos(double);
double asin(double);
double atan(double);
double atan2(double, double);
double cos(double);
double sin(double);
double tan(double);

double cosh(double);
double sinh(double);
double tanh(double);

double exp(double);
double frexp(double, int *);
double ldexp(double, int);
double log(double);
double log10(double);
double modf(double, double *);

double pow(double, double);
double sqrt(double);

double ceil(double);
double fabs(double) __MATH_CONST;
double floor(double);
double fmod(double, double);

#if __BSD_VISIBLE || __XPG_VISIBLE
double erf(double);
double erfc(double);
double gamma(double);
double hypot(double, double);
int finite(double) __MATH_CONST;
double j0(double);
double j1(double);
double jn(int, double);
double lgamma(double);
double y0(double);
double y1(double);
double yn(int, double);

#if __XPG_VISIBLE >= 400 || __BSD_VISIBLE
double acosh(double);
double asinh(double);
double atanh(double);
double cbrt(double);
double expm1(double);
int ilogb(double) __MATH_CONST;
double log1p(double);
double logb(double);
double nextafter(double, double);
double remainder(double, double);
double rint(double);
double scalb(double, double);
#endif /* __XPG_VISIBLE >= 400 || __BSD_VISIBLE */

#if __XPG_VISIBLE >= 500 || __BSD_VISIBLE
double copysign(double, double) __MATH_CONST;
double fdim(double, double);
double fmax(double, double) __MATH_CONST;
double fmin(double, double) __MATH_CONST;
double nan(const char *) __MATH_CONST;
double nearbyint(double);
double round(double);
double scalbln(double, long);
double scalbn(double, int);
double tgamma(double);
double trunc(double);
#endif /* __XPG_VISIBLE >= 500 || __BSD_VISIBLE */
#endif /* __BSD_VISIBLE || __XPG_VISIBLE */

#if __ISO_C_VISIBLE >= 1999
double fma(double, double, double);

int __fpclassify(double) __MATH_CONST;
int __fpclassifyf(float) __MATH_CONST;
int __fpclassifyl(long double) __MATH_CONST;
int __isfinite(double) __MATH_CONST;
int __isfinitef(float) __MATH_CONST;
int __isfinitel(long double) __MATH_CONST;
int __isinf(double) __MATH_CONST;
int __isinff(float) __MATH_CONST;
int __isinfl(long double) __MATH_CONST;
int __isnan(double) __MATH_CONST;
int __isnanf(float) __MATH_CONST;
int __isnanl(long double) __MATH_CONST;
int __isnormal(double) __MATH_CONST;
int __isnormalf(float) __MATH_CONST;
int __isnormall(long double) __MATH_CONST;
int __signbit(double) __MATH_CONST;
int __signbitf(float) __MATH_CONST;
int __signbitl(long double) __MATH_CONST;

double acosh(double);
double asinh(double);
double atanh(double);

double exp2(double);
double expm1(double);
int ilogb(double) __MATH_CONST;
double log1p(double);
double log2(double);
double logb(double);
double scalbn(double, int);
double scalbln(double, long);

double cbrt(double);
double hypot(double, double);

double erf(double);
double erfc(double);
double lgamma(double);
double tgamma(double);

double nearbyint(double);
double rint(double);
long lrint(double);
long long llrint(double);
double round(double);
long lround(double);
long long llround(double);
double trunc(double);

double remainder(double, double);
double remquo(double, double, int *);

double copysign(double, double) __MATH_CONST;
double nan(const char *) __MATH_CONST;
double nextafter(double, double);
double nexttoward(double, long double);

double fdim(double, double);
double fmax(double, double) __MATH_CONST;
double fmin(double, double) __MATH_CONST;

double fma(double, double, double);

/*
 * Float versions of C99 functions
 */
float acosf(float);
float asinf(float);
float atanf(float);
float atan2f(float, float);
float cosf(float);
float sinf(float);
float tanf(float);

float acoshf(float);
float asinhf(float);
float atanhf(float);
float coshf(float);
float sinhf(float);
float tanhf(float);

float expf(float);
float exp2f(float);
float expm1f(float);
float frexpf(float, int *);
int ilogbf(float) __MATH_CONST;
float ldexpf(float, int);
float logf(float);
float log10f(float);
float log1pf(float);
float log2f(float);
float logbf(float);
float modff(float, float *);
float scalbnf(float, int);
float scalblnf(float, long);

float cbrtf(float);
float fabsf(float) __MATH_CONST;
float hypotf(float, float);
float powf(float, float);
float sqrtf(float);

float erff(float);
float erfcf(float);
float lgammaf(float);
float tgammaf(float);

float ceilf(float);
float floorf(float);
float nearbyintf(float);
float rintf(float);
long lrintf(float);
long long llrintf(float);
float roundf(float);
long lroundf(float);
long long llroundf(float);
float truncf(float);

float fmodf(float, float);
float remainderf(float, float);
float remquof(float, float, int *);

float copysignf(float, float) __MATH_CONST;
float nanf(const char *) __MATH_CONST;
float nextafterf(float, float);
float nexttowardf(float, long double);

float fdimf(float, float);
float fmaxf(float, float) __MATH_CONST;
float fminf(float, float) __MATH_CONST;

float fmaf(float, float, float);

/*
 * Long double versions of C99 functions
 */
long double acosl(long double);
long double asinl(long double);
long double atanl(long double);
long double atan2l(long double, long double);
long double cosl(long double);
long double sinl(long double);
long double tanl(long double);

long double acoshl(long double);
long double asinhl(long double);
long double atanhl(long double);
long double coshl(long double);
long double sinhl(long double);
long double tanhl(long double);

long double expl(long double);
long double exp2l(long double);
long double expm1l(long double);
long double frexpl(long double, int *);
int ilogbl(long double) __MATH_CONST;
long double ldexpl(long double, int);
long double logl(long double);
long double log10l(long double);
long double log1pl(long double);
long double log2l(long double);
long double logbl(long double);
long double modfl(long double, long double *);
long double scalbnl(long double, int);
long double scalblnl(long double, long);

long double cbrtl(long double);
long double fabsl(long double) __MATH_CONST;
long double hypotl(long double, long double);
long double powl(long double, long double);
long double sqrtl(long double);

long double erfl(long double);
long double erfcl(long double);
long double lgammal(long double);
long double tgammal(long double);

long double ceill(long double);
long double floorl(long double);
long double nearbyintl(long double);
long double rintl(long double);
long lrintl(long double);
long long llrintl(long double);
long double roundl(long double);
long lroundl(long double);
long long llroundl(long double);
long double truncl(long double);

long double fmodl(long double, long double);
long double remainderl(long double, long double);
long double remquol(long double, long double, int *);

long double copysignl(long double, long double) __MATH_CONST;
long double nanl(const char *) __MATH_CONST;
long double nextafterl(long double, long double);
long double nexttowardl(long double, long double);

long double fdiml(long double, long double);
long double fmaxl(long double, long double) __MATH_CONST;
long double fminl(long double, long double) __MATH_CONST;

long double fmal(long double, long double, long double);

#define fpclassify(x)                                                          \
	((sizeof(x) == sizeof(float))                                          \
	     ? __fpclassifyf(x)                                                \
	     : (sizeof(x) == sizeof(double)) ? __fpclassify(x)                 \
	                                     : __fpclassifyl(x))

#define isfinite(x)                                                            \
	((sizeof(x) == sizeof(float))                                          \
	     ? __isfinitef(x)                                                  \
	     : (sizeof(x) == sizeof(double)) ? __isfinite(x) : __isfinitel(x))

#define isinf(x)                                                               \
	((sizeof(x) == sizeof(float))                                          \
	     ? __isinff(x)                                                     \
	     : (sizeof(x) == sizeof(double)) ? __isinf(x) : __isinfl(x))

#define isnan(x)                                                               \
	((sizeof(x) == sizeof(float))                                          \
	     ? __isnanf(x)                                                     \
	     : (sizeof(x) == sizeof(double)) ? __isnan(x) : __isnanl(x))

#define isnormal(x)                                                            \
	((sizeof(x) == sizeof(float))                                          \
	     ? __isnormalf(x)                                                  \
	     : (sizeof(x) == sizeof(double)) ? __isnormal(x) : __isnormall(x))

#define signbit(x)                                                             \
	((sizeof(x) == sizeof(float))                                          \
	     ? __signbitf(x)                                                   \
	     : (sizeof(x) == sizeof(double)) ? __signbit(x) : __signbitl(x))

#define isgreater(x, y) (!isunordered((x), (y)) && (x) > (y))
#define isgreaterequal(x, y) (!isunordered((x), (y)) && (x) >= (y))
#define isless(x, y) (!isunordered((x), (y)) && (x) < (y))
#define islessequal(x, y) (!isunordered((x), (y)) && (x) <= (y))
#define islessgreater(x, y) (!isunordered((x), (y)) && ((x) > (y) || (y) > (x)))
#define isunordered(x, y) (isnan(x) || isnan(y))
#endif /* __ISO_C_VISIBLE >= 1999 */

#if __BSD_VISIBLE
double drem(double, double);
double gamma_r(double, int *);
double lgamma_r(double, int *);

float dremf(float, float);
float gammaf_r(float, int *);
float lgammaf_r(float, int *);

extern int signgam;
#endif /* __BSD_VISIBLE */

__END_DECLS

#endif /* !_MATH_H_ */