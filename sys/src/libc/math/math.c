/*
 * Single core-responding file because math is already enough of an mess to manage
 */

#include <sys/types.h>
#include <math.h>
#include <float.h>
#include <stdint.h>
#include <limits.h>

const union __infinity_un __infinity = {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x7f}};
const union __nan_un __nan = {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0x7f}};

int signgam = 0;

typedef union {
	double value;
	struct {
		uint32_t lsw;
		uint32_t msw;
	} parts;
	uint64_t word;
} ieee_double_shape_type;

typedef union {
	float value;
	uint32_t word;
} ieee_float_shape_type;

#define EXTRACT_WORDS(ix0, ix1, d)                                             \
	do {                                                                   \
		ieee_double_shape_type ew_u;                                   \
		ew_u.value = (d);                                              \
		(ix0) = ew_u.parts.msw;                                        \
		(ix1) = ew_u.parts.lsw;                                        \
	} while (0)

#define GET_HIGH_WORD(i, d)                                                    \
	do {                                                                   \
		ieee_double_shape_type gh_u;                                   \
		gh_u.value = (d);                                              \
		(i) = gh_u.parts.msw;                                          \
	} while (0)

#define GET_LOW_WORD(i, d)                                                     \
	do {                                                                   \
		ieee_double_shape_type gl_u;                                   \
		gl_u.value = (d);                                              \
		(i) = gl_u.parts.lsw;                                          \
	} while (0)

#define SET_HIGH_WORD(d, v)                                                    \
	do {                                                                   \
		ieee_double_shape_type sh_u;                                   \
		sh_u.value = (d);                                              \
		sh_u.parts.msw = (v);                                          \
		(d) = sh_u.value;                                              \
	} while (0)

#define SET_LOW_WORD(d, v)                                                     \
	do {                                                                   \
		ieee_double_shape_type sl_u;                                   \
		sl_u.value = (d);                                              \
		sl_u.parts.lsw = (v);                                          \
		(d) = sl_u.value;                                              \
	} while (0)

#define INSERT_WORDS(d, ix0, ix1)                                              \
	do {                                                                   \
		ieee_double_shape_type iw_u;                                   \
		iw_u.parts.msw = (ix0);                                        \
		iw_u.parts.lsw = (ix1);                                        \
		(d) = iw_u.value;                                              \
	} while (0)

#define GET_FLOAT_WORD(i, d)                                                   \
	do {                                                                   \
		ieee_float_shape_type gf_u;                                    \
		gf_u.value = (d);                                              \
		(i) = gf_u.word;                                               \
	} while (0)

#define SET_FLOAT_WORD(d, i)                                                   \
	do {                                                                   \
		ieee_float_shape_type sf_u;                                    \
		sf_u.word = (i);                                               \
		(d) = sf_u.value;                                              \
	} while (0)

int
__fpclassify(double x)
{
	uint32_t hx, lx;
	EXTRACT_WORDS(hx, lx, x);
	hx &= 0x7fffffff;
	
	if ((hx | lx) == 0)
		return FP_ZERO;
	if (hx < 0x00100000) {
		return FP_SUBNORMAL;
	}
	if (hx >= 0x7ff00000) {
		if (((hx & 0x000fffff) | lx) == 0)
			return FP_INFINITE;
		return FP_NAN;
	}
	return FP_NORMAL;
}

int
__fpclassifyf(float x)
{
	uint32_t w;
	GET_FLOAT_WORD(w, x);
	w &= 0x7fffffff;
	
	if (w == 0)
		return FP_ZERO;
	if (w < 0x00800000)
		return FP_SUBNORMAL;
	if (w >= 0x7f800000) {
		if (w == 0x7f800000)
			return FP_INFINITE;
		return FP_NAN;
	}
	return FP_NORMAL;
}

int
__fpclassifyl(long double x)
{
	return __fpclassify((double)x);
}

int
__isfinite(double x)
{
	uint32_t hx;
	GET_HIGH_WORD(hx, x);
	return (hx & 0x7ff00000) != 0x7ff00000;
}

int
__isfinitef(float x)
{
	uint32_t w;
	GET_FLOAT_WORD(w, x);
	return (w & 0x7f800000) != 0x7f800000;
}

int
__isfinitel(long double x)
{
	return __isfinite((double)x);
}

int
__isinf(double x)
{
	uint32_t hx, lx;
	EXTRACT_WORDS(hx, lx, x);
	hx &= 0x7fffffff;
	hx |= (uint32_t)(lx | (-lx)) >> 31;
	hx = 0x7ff00000 - hx;
	return 1 - (int)((uint32_t)(hx | (-hx)) >> 31);
}

int
__isinff(float x)
{
	uint32_t w;
	GET_FLOAT_WORD(w, x);
	w &= 0x7fffffff;
	return w == 0x7f800000;
}

int
__isinfl(long double x)
{
	return __isinf((double)x);
}

int
__isnan(double x)
{
	uint32_t hx, lx;
	EXTRACT_WORDS(hx, lx, x);
	hx &= 0x7fffffff;
	hx |= (uint32_t)(lx | (-lx)) >> 31;
	hx = 0x7ff00000 - hx;
	return (int)((uint32_t)(hx) >> 31);
}

int
__isnanf(float x)
{
	uint32_t w;
	GET_FLOAT_WORD(w, x);
	w &= 0x7fffffff;
	return w > 0x7f800000;
}

int
__isnanl(long double x)
{
	return __isnan((double)x);
}

int
__isnormal(double x)
{
	uint32_t hx;
	GET_HIGH_WORD(hx, x);
	hx &= 0x7ff00000;
	return hx != 0 && hx != 0x7ff00000;
}

int
__isnormalf(float x)
{
	uint32_t w;
	GET_FLOAT_WORD(w, x);
	w &= 0x7f800000;
	return w != 0 && w != 0x7f800000;
}

int
__isnormall(long double x)
{
	return __isnormal((double)x);
}

int
__signbit(double x)
{
	uint32_t hx;
	GET_HIGH_WORD(hx, x);
	return (hx & 0x80000000) != 0;
}

int
__signbitf(float x)
{
	uint32_t w;
	GET_FLOAT_WORD(w, x);
	return (w & 0x80000000) != 0;
}

int
__signbitl(long double x)
{
	return __signbit((double)x);
}

/* === Basic Operations === */

double
fabs(double x)
{
	uint32_t hx;
	GET_HIGH_WORD(hx, x);
	SET_HIGH_WORD(x, hx & 0x7fffffff);
	return x;
}

float
fabsf(float x)
{
	uint32_t w;
	GET_FLOAT_WORD(w, x);
	SET_FLOAT_WORD(x, w & 0x7fffffff);
	return x;
}

long double
fabsl(long double x)
{
	return (long double)fabs((double)x);
}

double
copysign(double x, double y)
{
	uint32_t hx, hy;
	GET_HIGH_WORD(hx, x);
	GET_HIGH_WORD(hy, y);
	SET_HIGH_WORD(x, (hx & 0x7fffffff) | (hy & 0x80000000));
	return x;
}

float
copysignf(float x, float y)
{
	uint32_t wx, wy;
	GET_FLOAT_WORD(wx, x);
	GET_FLOAT_WORD(wy, y);
	SET_FLOAT_WORD(x, (wx & 0x7fffffff) | (wy & 0x80000000));
	return x;
}

long double
copysignl(long double x, long double y)
{
	return (long double)copysign((double)x, (double)y);
}

/* === Rounding Functions === */

double
floor(double x)
{
	int32_t i0, i1, j0;
	uint32_t i, j;
	
	EXTRACT_WORDS(i0, i1, x);
	j0 = ((i0 >> 20) & 0x7ff) - 0x3ff;
	
	if (j0 < 20) {
		if (j0 < 0) {
			if (((i0 & 0x7fffffff) | i1) == 0)
				return x;
			if (i0 >= 0) {
				i0 = i1 = 0;
			} else if (((i0 & 0x7fffffff) | i1) != 0) {
				i0 = 0xbff00000;
				i1 = 0;
			}
		} else {
			i = (0x000fffff) >> j0;
			if (((i0 & i) | i1) == 0)
				return x;
			if (i0 < 0)
				i0 += (0x00100000) >> j0;
			i0 &= (~i);
			i1 = 0;
		}
	} else if (j0 > 51) {
		if (j0 == 0x400)
			return x + x;
		else
			return x;
	} else {
		i = ((uint32_t)(0xffffffff)) >> (j0 - 20);
		if ((i1 & i) == 0)
			return x;
		if (i0 < 0)
			i1 += (1 << (52 - j0));
		i1 &= (~i);
	}
	INSERT_WORDS(x, i0, i1);
	return x;
}

float
floorf(float x)
{
	int32_t i0, j0;
	uint32_t i;
	
	GET_FLOAT_WORD(i0, x);
	j0 = ((i0 >> 23) & 0xff) - 0x7f;
	
	if (j0 < 23) {
		if (j0 < 0) {
			if ((i0 & 0x7fffffff) == 0)
				return x;
			if (i0 >= 0) {
				i0 = 0;
			} else if ((i0 & 0x7fffffff) != 0) {
				i0 = 0xbf800000;
			}
		} else {
			i = (0x007fffff) >> j0;
			if ((i0 & i) == 0)
				return x;
			if (i0 < 0)
				i0 += (0x00800000) >> j0;
			i0 &= (~i);
		}
		SET_FLOAT_WORD(x, i0);
		return x;
	}
	if (j0 == 0x80)
		return x + x;
	return x;
}

long double
floorl(long double x)
{
	return (long double)floor((double)x);
}

double
ceil(double x)
{
	int32_t i0, i1, j0;
	uint32_t i, j;
	
	EXTRACT_WORDS(i0, i1, x);
	j0 = ((i0 >> 20) & 0x7ff) - 0x3ff;
	
	if (j0 < 20) {
		if (j0 < 0) {
			if (((i0 & 0x7fffffff) | i1) == 0)
				return x;
			if (i0 < 0) {
				i0 = 0x80000000;
				i1 = 0;
			} else if (((i0 & 0x7fffffff) | i1) != 0) {
				i0 = 0x3ff00000;
				i1 = 0;
			}
		} else {
			i = (0x000fffff) >> j0;
			if (((i0 & i) | i1) == 0)
				return x;
			if (i0 > 0)
				i0 += (0x00100000) >> j0;
			i0 &= (~i);
			i1 = 0;
		}
	} else if (j0 > 51) {
		if (j0 == 0x400)
			return x + x;
		else
			return x;
	} else {
		i = ((uint32_t)(0xffffffff)) >> (j0 - 20);
		if ((i1 & i) == 0)
			return x;
		if (i0 > 0)
			i1 += (1 << (52 - j0));
		i1 &= (~i);
	}
	INSERT_WORDS(x, i0, i1);
	return x;
}

float
ceilf(float x)
{
	int32_t i0, j0;
	uint32_t i;
	
	GET_FLOAT_WORD(i0, x);
	j0 = ((i0 >> 23) & 0xff) - 0x7f;
	
	if (j0 < 23) {
		if (j0 < 0) {
			if ((i0 & 0x7fffffff) == 0)
				return x;
			if (i0 < 0) {
				i0 = 0x80000000;
			} else if ((i0 & 0x7fffffff) != 0) {
				i0 = 0x3f800000;
			}
		} else {
			i = (0x007fffff) >> j0;
			if ((i0 & i) == 0)
				return x;
			if (i0 > 0)
				i0 += (0x00800000) >> j0;
			i0 &= (~i);
		}
		SET_FLOAT_WORD(x, i0);
		return x;
	}
	if (j0 == 0x80)
		return x + x;
	return x;
}

long double
ceill(long double x)
{
	return (long double)ceil((double)x);
}

double
trunc(double x)
{
	int32_t i0, i1, j0;
	uint32_t i;
	
	EXTRACT_WORDS(i0, i1, x);
	j0 = ((i0 >> 20) & 0x7ff) - 0x3ff;
	
	if (j0 < 20) {
		if (j0 < 0) {
			INSERT_WORDS(x, i0 & 0x80000000, 0);
		} else {
			i = (0x000fffff) >> j0;
			if (((i0 & i) | i1) == 0)
				return x;
			INSERT_WORDS(x, i0 & (~i), 0);
		}
	} else if (j0 > 51) {
		if (j0 == 0x400)
			return x + x;
		else
			return x;
	} else {
		i = ((uint32_t)(0xffffffff)) >> (j0 - 20);
		if ((i1 & i) == 0)
			return x;
		INSERT_WORDS(x, i0, i1 & (~i));
	}
	return x;
}

float
truncf(float x)
{
	int32_t i0, j0;
	uint32_t i;
	
	GET_FLOAT_WORD(i0, x);
	j0 = ((i0 >> 23) & 0xff) - 0x7f;
	
	if (j0 < 23) {
		if (j0 < 0) {
			SET_FLOAT_WORD(x, i0 & 0x80000000);
		} else {
			i = (0x007fffff) >> j0;
			if ((i0 & i) == 0)
				return x;
			SET_FLOAT_WORD(x, i0 & (~i));
		}
	} else {
		if (j0 == 0x80)
			return x + x;
	}
	return x;
}

long double
truncl(long double x)
{
	return (long double)trunc((double)x);
}

double
round(double x)
{
	double t;
	
	if (!__isfinite(x))
		return x;
	
	if (x >= 0.0) {
		t = floor(x);
		if (t - x <= -0.5)
			t += 1.0;
		return t;
	} else {
		t = floor(-x);
		if (t + x <= -0.5)
			t += 1.0;
		return -t;
	}
}

float
roundf(float x)
{
	return (float)round((double)x);
}

long double
roundl(long double x)
{
	return (long double)round((double)x);
}

long
lround(double x)
{
	return (long)round(x);
}

long
lroundf(float x)
{
	return (long)roundf(x);
}

long
lroundl(long double x)
{
	return (long)roundl(x);
}

long long
llround(double x)
{
	return (long long)round(x);
}

long long
llroundf(float x)
{
	return (long long)roundf(x);
}

long long
llroundl(long double x)
{
	return (long long)roundl(x);
}

double
rint(double x)
{
	return nearbyint(x);
}

float
rintf(float x)
{
	return nearbyintf(x);
}

long double
rintl(long double x)
{
	return nearbyintl(x);
}

long
lrint(double x)
{
	return (long)rint(x);
}

long
lrintf(float x)
{
	return (long)rintf(x);
}

long
lrintl(long double x)
{
	return (long)rintl(x);
}

long long
llrint(double x)
{
	return (long long)rint(x);
}

long long
llrintf(float x)
{
	return (long long)rintf(x);
}

long long
llrintl(long double x)
{
	return (long long)rintl(x);
}

double
nearbyint(double x)
{
	int32_t i0, i1, j0, sx;
	uint32_t i, i1_mod;
	double w, t;
	
	EXTRACT_WORDS(i0, i1, x);
	sx = (i0 >> 31) & 1;
	j0 = ((i0 >> 20) & 0x7ff) - 0x3ff;
	
	if (j0 < 20) {
		if (j0 < 0) {
			if (((i0 & 0x7fffffff) | i1) == 0)
				return x;
			i1_mod = (i1 | (i0 & 0x0fffff));
			i0 &= 0xfffe0000;
			i0 |= ((i1_mod | -i1_mod) >> 12) & 0x80000;
			SET_HIGH_WORD(x, i0);
			w = 8.9884656743115795386e+307 + x;
			t = w - 8.9884656743115795386e+307;
			GET_HIGH_WORD(i0, t);
			SET_HIGH_WORD(t, (i0 & 0x7fffffff) | (sx << 31));
			return t;
		} else {
			i = (0x000fffff) >> j0;
			if (((i0 & i) | i1) == 0)
				return x;
			i >>= 1;
			if (((i0 & i) | i1) != 0) {
				if (j0 == 19)
					i1 = 0x40000000;
				else
					i0 = (i0 & (~i)) | ((0x00020000) >> j0);
			}
		}
	} else if (j0 > 51) {
		if (j0 == 0x400)
			return x + x;
		else
			return x;
	} else {
		i = ((uint32_t)(0xffffffff)) >> (j0 - 20);
		if ((i1 & i) == 0)
			return x;
		i >>= 1;
		if ((i1 & i) != 0)
			i1 = (i1 & (~i)) | ((0x40000000) >> (j0 - 20));
	}
	INSERT_WORDS(x, i0, i1);
	w = 8.9884656743115795386e+307 + x;
	return w - 8.9884656743115795386e+307;
}

float
nearbyintf(float x)
{
	return (float)nearbyint((double)x);
}

long double
nearbyintl(long double x)
{
	return (long double)nearbyint((double)x);
}

/* === fmod and remainder === */

double
fmod(double x, double y)
{
	int32_t n, hx, hy, hz, ix, iy, sx, i;
	uint32_t lx, ly, lz;
	
	EXTRACT_WORDS(hx, lx, x);
	EXTRACT_WORDS(hy, ly, y);
	sx = hx & 0x80000000;
	hx ^= sx;
	hy &= 0x7fffffff;
	
	if ((hy | ly) == 0 || (hx >= 0x7ff00000) ||
	    ((hy | ((ly | -ly) >> 31)) > 0x7ff00000))
		return (x * y) / (x * y);
	
	if (hx <= hy) {
		if ((hx < hy) || (lx < ly))
			return x;
		if (lx == ly)
			return 0.0 * x;
	}
	
	if (hx < 0x00100000) {
		if (hx == 0) {
			for (ix = -1043, i = lx; i > 0; i <<= 1)
				ix -= 1;
		} else {
			for (ix = -1022, i = (hx << 11); i > 0; i <<= 1)
				ix -= 1;
		}
	} else
		ix = (hx >> 20) - 1023;
	
	if (hy < 0x00100000) {
		if (hy == 0) {
			for (iy = -1043, i = ly; i > 0; i <<= 1)
				iy -= 1;
		} else {
			for (iy = -1022, i = (hy << 11); i > 0; i <<= 1)
				iy -= 1;
		}
	} else
		iy = (hy >> 20) - 1023;
	
	if (ix >= -1022)
		hx = 0x00100000 | (0x000fffff & hx);
	else {
		n = -1022 - ix;
		if (n <= 31) {
			hx = (hx << n) | (lx >> (32 - n));
			lx <<= n;
		} else {
			hx = lx << (n - 32);
			lx = 0;
		}
	}
	
	if (iy >= -1022)
		hy = 0x00100000 | (0x000fffff & hy);
	else {
		n = -1022 - iy;
		if (n <= 31) {
			hy = (hy << n) | (ly >> (32 - n));
			ly <<= n;
		} else {
			hy = ly << (n - 32);
			ly = 0;
		}
	}
	
	n = ix - iy;
	while (n--) {
		hz = hx - hy;
		lz = lx - ly;
		if (lx < ly)
			hz -= 1;
		if (hz < 0) {
			hx = hx + hx + (lx >> 31);
			lx = lx + lx;
		} else {
			if ((hz | lz) == 0)
				return 0.0 * x;
			hx = hz + hz + (lz >> 31);
			lx = lz + lz;
		}
	}
	hz = hx - hy;
	lz = lx - ly;
	if (lx < ly)
		hz -= 1;
	if (hz >= 0) {
		hx = hz;
		lx = lz;
	}
	
	if ((hx | lx) == 0)
		return 0.0 * x;
	while (hx < 0x00100000) {
		hx = hx + hx + (lx >> 31);
		lx = lx + lx;
		iy -= 1;
	}
	if (iy >= -1022) {
		hx = ((hx - 0x00100000) | ((iy + 1023) << 20));
		INSERT_WORDS(x, hx | sx, lx);
	} else {
		n = -1022 - iy;
		if (n <= 20) {
			lx = (lx >> n) | ((uint32_t)hx << (32 - n));
			hx >>= n;
		} else if (n <= 31) {
			lx = (hx << (32 - n)) | (lx >> n);
			hx = sx;
		} else {
			lx = hx >> (n - 32);
			hx = sx;
		}
		INSERT_WORDS(x, hx | sx, lx);
		x *= 1.0;
	}
	return x;
}

float
fmodf(float x, float y)
{
	return (float)fmod((double)x, (double)y);
}

long double
fmodl(long double x, long double y)
{
	return (long double)fmod((double)x, (double)y);
}

double
remainder(double x, double y)
{
	int32_t hx, hy, sx, i;
	uint32_t lx, ly;
	double y_half;
	
	EXTRACT_WORDS(hx, lx, x);
	EXTRACT_WORDS(hy, ly, y);
	sx = hx & 0x80000000;
	
	hy &= 0x7fffffff;
	hx &= 0x7fffffff;
	
	if ((hy | ly) == 0 || (hx >= 0x7ff00000) ||
	    ((hy | ((ly | -ly) >> 31)) > 0x7ff00000))
		return (x * y) / (x * y);
	
	if (hx <= hy) {
		if ((hx < hy) || (lx < ly))
			return x;
		if (lx == ly)
			return 0.0 * x;
	}
	
	x = fabs(x);
	y = fabs(y);
	
	if (hx < hy || (hx == hy && lx < ly)) {
		if (hx == hy && lx == ly)
			return 0.0 * x;
		return x;
	}
	
	y_half = 0.5 * y;
	if (x > y_half) {
		x -= y;
		if (x >= y_half)
			x -= y;
	}
	
	GET_HIGH_WORD(hx, x);
	SET_HIGH_WORD(x, hx ^ sx);
	return x;
}

float
remainderf(float x, float y)
{
	return (float)remainder((double)x, (double)y);
}

long double
remainderl(long double x, long double y)
{
	return (long double)remainder((double)x, (double)y);
}

double
remquo(double x, double y, int *quo)
{
	int32_t hx, hy, sx, sy;
	uint32_t lx, ly;
	int q;
	double result;
	
	EXTRACT_WORDS(hx, lx, x);
	EXTRACT_WORDS(hy, ly, y);
	
	sx = hx & 0x80000000;
	sy = hy & 0x80000000;
	
	q = 0;
	result = remainder(x, y);
	
	/* Simplified quotient calculation */
	if (fabs(x) >= fabs(y)) {
		q = (int)(x / y);
		q &= 0x7f;
	}
	
	if (sx != sy)
		q = -q;
	
	*quo = q;
	return result;
}

float
remquof(float x, float y, int *quo)
{
	return (float)remquo((double)x, (double)y, quo);
}

long double
remquol(long double x, long double y, int *quo)
{
	return (long double)remquo((double)x, (double)y, quo);
}

double
drem(double x, double y)
{
	return remainder(x, y);
}

float
dremf(float x, float y)
{
	return remainderf(x, y);
}

/* === Square root === */

double
sqrt(double x)
{
	double result;
	
	if (x < 0.0)
		return NAN;
	
	if (!__isfinite(x) || x == 0.0)
		return x;
	
	/* Use hardware sqrt instruction */
	__asm__ volatile("sqrtsd %1, %0" : "=x"(result) : "x"(x));
	return result;
}

float
sqrtf(float x)
{
	float result;
	
	if (x < 0.0f)
		return NAN;
	
	if (!__isfinitef(x) || x == 0.0f)
		return x;
	
	__asm__ volatile("sqrtss %1, %0" : "=x"(result) : "x"(x));
	return result;
}

long double
sqrtl(long double x)
{
	return (long double)sqrt((double)x);
}

double
cbrt(double x)
{
	static const double B1 = 715094163, B2 = 696219795;
	static const double C = 5.42857142857142815906e-01,
	                    D = -7.05306122448979611050e-01,
	                    E = 1.41428571428571436819e+00,
	                    F = 1.60714285714285720630e+00,
	                    G = 3.57142857142857150787e-01;
	int32_t hx;
	double r, s, t = 0.0, w;
	uint32_t sign, high, low;
	
	EXTRACT_WORDS(hx, low, x);
	sign = hx & 0x80000000;
	hx ^= sign;
	
	if (hx >= 0x7ff00000)
		return (x + x);
	
	if ((hx | low) == 0)
		return (x);
	
	SET_HIGH_WORD(t, sign | 0x3fe00000);
	SET_LOW_WORD(t, 0);
	
	SET_HIGH_WORD(x, hx);
	
	r = (t * t) * (t / x);
	r = (r - (B1 + B2) / B2 * r * (C + r * (D + r * (E + r * (F + r * G)))));
	t = r * t;
	GET_HIGH_WORD(high, t);
	INSERT_WORDS(t, high + 0x00100000 + ((sign >> 2) & 0x40000000), 0);
	
	s = t * t;
	r = x / s;
	w = t + t;
	r = (r - t) / (w + r);
	t = t + t * r;
	
	GET_HIGH_WORD(high, t);
	INSERT_WORDS(t, sign | high, 0);
	return (t);
}

float
cbrtf(float x)
{
	return (float)cbrt((double)x);
}

long double
cbrtl(long double x)
{
	return (long double)cbrt((double)x);
}

/* === Exponential and logarithm === */

static const double
    ln2HI = 6.93147180369123816490e-01,
    ln2LO = 1.90821492927058770002e-10,
    invln2 = 1.44269504088896338700e+00,
    P1 = 1.66666666666666019037e-01,
    P2 = -2.77777777770155933842e-03,
    P3 = 6.61375632143793436117e-05,
    P4 = -1.65339022054652515390e-06,
    P5 = 4.13813679705723846039e-08;

double
exp(double x)
{
	double hi, lo, c, t, y;
	int32_t k, xsb;
	uint32_t hx;
	
	GET_HIGH_WORD(hx, x);
	xsb = (hx >> 31) & 1;
	hx &= 0x7fffffff;
	
	if (hx >= 0x40862E42) {
		if (hx >= 0x7ff00000) {
			if (((hx & 0xfffff) | xsb) == 0)
				return (xsb == 0) ? x : 0.0;
			return x + x;
		}
		if (x > 709.782712893383973096)
			return HUGE_VAL;
		if (x < -745.13321910194110842)
			return 0.0;
	}
	
	if (hx > 0x3fd62e42) {
		if (hx < 0x3FF0A2B2)
			k = 0;
		else {
			k = (int)(invln2 * x + ((xsb == 0) ? 0.5 : -0.5));
			t = k;
			hi = x - t * ln2HI;
			lo = t * ln2LO;
			x = hi - lo;
		}
	} else if (hx < 0x3e300000) {
		if (HUGE_VAL + x > 1.0)
			return 1.0 + x;
	} else
		k = 0;
	
	t = x * x;
	c = x - t * (P1 + t * (P2 + t * (P3 + t * (P4 + t * P5))));
	if (k == 0)
		return 1.0 - ((x * c) / (c - 2.0) - x);
	else
		y = 1.0 - ((lo - (x * c) / (2.0 - c)) - hi);
	
	if (k >= -1021) {
		uint32_t hy;
		GET_HIGH_WORD(hy, y);
		SET_HIGH_WORD(y, hy + (k << 20));
		return y;
	} else {
		uint32_t hy;
		GET_HIGH_WORD(hy, y);
		SET_HIGH_WORD(y, hy + ((k + 1000) << 20));
		return y * 2.2250738585072014e-308;
	}
}

float
expf(float x)
{
	return (float)exp((double)x);
}

long double
expl(long double x)
{
	return (long double)exp((double)x);
}

static const double
    Lg1 = 6.666666666666735130e-01,
    Lg2 = 3.999999999940941908e-01,
    Lg3 = 2.857142874366239149e-01,
    Lg4 = 2.222219843214978396e-01,
    Lg5 = 1.818357216161805012e-01,
    Lg6 = 1.531383769920937332e-01,
    Lg7 = 1.479819860511658591e-01;

double
log(double x)
{
	double hfsq, f, s, z, R, w, t1, t2, dk;
	int32_t k, hx, i, j;
	uint32_t lx;
	
	EXTRACT_WORDS(hx, lx, x);
	
	k = 0;
	if (hx < 0x00100000) {
		if (((hx & 0x7fffffff) | lx) == 0)
			return -HUGE_VAL;
		if (hx < 0)
			return NAN;
		k -= 54;
		x *= 1.80143985094819840000e+16;
		GET_HIGH_WORD(hx, x);
	}
	if (hx >= 0x7ff00000)
		return x + x;
	k += (hx >> 20) - 1023;
	hx &= 0x000fffff;
	i = (hx + 0x95f64) & 0x100000;
	SET_HIGH_WORD(x, hx | (i ^ 0x3ff00000));
	k += (i >> 20);
	f = x - 1.0;
	
	if ((0x000fffff & (2 + hx)) < 3) {
		if (f == 0.0) {
			if (k == 0)
				return 0.0;
			else {
				dk = (double)k;
				return dk * ln2HI + dk * ln2LO;
			}
		}
		R = f * f * (0.5 - 0.33333333333333333 * f);
		if (k == 0)
			return f - R;
		else {
			dk = (double)k;
			return dk * ln2HI - ((R - dk * ln2LO) - f);
		}
	}
	s = f / (2.0 + f);
	dk = (double)k;
	z = s * s;
	i = hx - 0x6147a;
	w = z * z;
	j = 0x6b851 - hx;
	t1 = w * (Lg2 + w * (Lg4 + w * Lg6));
	t2 = z * (Lg1 + w * (Lg3 + w * (Lg5 + w * Lg7)));
	i |= j;
	R = t2 + t1;
	if (i > 0) {
		hfsq = 0.5 * f * f;
		if (k == 0)
			return f - (hfsq - s * (hfsq + R));
		else
			return dk * ln2HI - ((hfsq - (s * (hfsq + R) + dk * ln2LO)) - f);
	} else {
		if (k == 0)
			return f - s * (f - R);
		else
			return dk * ln2HI - ((s * (f - R) - dk * ln2LO) - f);
	}
}

float
logf(float x)
{
	return (float)log((double)x);
}

long double
logl(long double x)
{
	return (long double)log((double)x);
}

static const double ivln10 = 4.34294481903251816668e-01;

double
log10(double x)
{
	double y, z;
	int32_t i, k, hx;
	uint32_t lx;
	
	EXTRACT_WORDS(hx, lx, x);
	
	k = 0;
	if (hx < 0x00100000) {
		if (((hx & 0x7fffffff) | lx) == 0)
			return -HUGE_VAL;
		if (hx < 0)
			return NAN;
		k -= 54;
		x *= 1.80143985094819840000e+16;
		GET_HIGH_WORD(hx, x);
	}
	if (hx >= 0x7ff00000)
		return x + x;
	k += (hx >> 20) - 1023;
	i = ((uint32_t)k & 0x80000000) >> 31;
	hx = (hx & 0x000fffff) | ((0x3ff - i) << 20);
	y = (double)(k + i);
	SET_HIGH_WORD(x, hx);
	z = y * 0.30102999566398119802 + ivln10 * log(x);
	return z;
}

float
log10f(float x)
{
	return (float)log10((double)x);
}

long double
log10l(long double x)
{
	return (long double)log10((double)x);
}

double
log2(double x)
{
	static const double ivln2 = 1.44269504088896338700e+00;
	return ivln2 * log(x);
}

float
log2f(float x)
{
	return (float)log2((double)x);
}

long double
log2l(long double x)
{
	return (long double)log2((double)x);
}

double
log1p(double x)
{
	static const double
	    ln2_hi = 6.93147180369123816490e-01,
	    ln2_lo = 1.90821492927058770002e-10,
	    Lp1 = 6.666666666666735130e-01,
	    Lp2 = 3.999999999940941908e-01,
	    Lp3 = 2.857142874366239149e-01,
	    Lp4 = 2.222219843214978396e-01,
	    Lp5 = 1.818357216161805012e-01,
	    Lp6 = 1.531383769920937332e-01,
	    Lp7 = 1.479819860511658591e-01;
	
	double hfsq, f, c, s, z, R, u;
	int32_t k, hx, hu, ax;
	
	GET_HIGH_WORD(hx, x);
	ax = hx & 0x7fffffff;
	
	k = 1;
	if (hx < 0x3FDA827A) {
		if (ax >= 0x3ff00000) {
			if (x == -1.0)
				return -HUGE_VAL;
			else
				return NAN;
		}
		if (ax < 0x3e200000) {
			if (ax < 0x3c900000)
				return x;
			return x - x * x * 0.5;
		}
		if (hx > 0 || hx <= ((int32_t)0xbfd2bec3)) {
			k = 0;
			f = x;
			hu = 1;
		}
	}
	if (hx >= 0x7ff00000)
		return x + x;
	if (k != 0) {
		if (hx < 0x43400000) {
			u = 1.0 + x;
			GET_HIGH_WORD(hu, u);
			k = (hu >> 20) - 1023;
			c = (k > 0) ? 1.0 - (u - x) : x - (u - 1.0);
			c /= u;
		} else {
			u = x;
			GET_HIGH_WORD(hu, u);
			k = (hu >> 20) - 1023;
			c = 0;
		}
		hu &= 0x000fffff;
		if (hu < 0x6a09e) {
			SET_HIGH_WORD(u, hu | 0x3ff00000);
		} else {
			k += 1;
			SET_HIGH_WORD(u, hu | 0x3fe00000);
			hu = (0x00100000 - hu) >> 2;
		}
		f = u - 1.0;
	}
	hfsq = 0.5 * f * f;
	if (hu == 0) {
		if (f == 0.0) {
			if (k == 0)
				return 0.0;
			else {
				c += k * ln2_lo;
				return k * ln2_hi + c;
			}
		}
		R = hfsq * (1.0 - 0.66666666666666666 * f);
		if (k == 0)
			return f - R;
		else
			return k * ln2_hi - ((R - (k * ln2_lo + c)) - f);
	}
	s = f / (2.0 + f);
	z = s * s;
	R = z * (Lp1 + z * (Lp2 + z * (Lp3 + z * (Lp4 + z * (Lp5 + z * (Lp6 + z * Lp7))))));
	if (k == 0)
		return f - (hfsq - s * (hfsq + R));
	else
		return k * ln2_hi - ((hfsq - (s * (hfsq + R) + (k * ln2_lo + c))) - f);
}

float
log1pf(float x)
{
	return (float)log1p((double)x);
}

long double
log1pl(long double x)
{
	return (long double)log1p((double)x);
}

double
exp2(double x)
{
	return pow(2.0, x);
}

float
exp2f(float x)
{
	return powf(2.0f, x);
}

long double
exp2l(long double x)
{
	return powl(2.0L, x);
}

double
expm1(double x)
{
	return exp(x) - 1.0;
}

float
expm1f(float x)
{
	return expf(x) - 1.0f;
}

long double
expm1l(long double x)
{
	return expl(x) - 1.0L;
}

/* === Power function === */

double
pow(double x, double y)
{
	double z, ax, z_h, z_l, p_h, p_l;
	double y1, t1, t2, r, s, t, u, v, w;
	int32_t i, j, k, yisint, n;
	int32_t hx, hy, ix, iy;
	uint32_t lx, ly;
	
	static const double
	    bp[] = {1.0, 1.5},
	    dp_h[] = {0.0, 5.84962487220764160156e-01},
	    dp_l[] = {0.0, 1.35003920212974897128e-08},
	    zero = 0.0,
	    one = 1.0,
	    two = 2.0,
	    two53 = 9007199254740992.0,
	    huge = 1.0e300,
	    tiny = 1.0e-300,
	    L1 = 5.99999999999994648725e-01,
	    L2 = 4.28571428578550184252e-01,
	    L3 = 3.33333329818377432918e-01,
	    L4 = 2.72728123808534006489e-01,
	    L5 = 2.30660745775561754067e-01,
	    L6 = 2.06975017800338417784e-01,
	    P1 = 1.66666666666666019037e-01,
	    P2 = -2.77777777770155933842e-03,
	    P3 = 6.61375632143793436117e-05,
	    P4 = -1.65339022054652515390e-06,
	    P5 = 4.13813679705723846039e-08,
	    lg2 = 6.93147180559945286227e-01,
	    lg2_h = 6.93147182464599609375e-01,
	    lg2_l = -1.90465429995776804525e-09,
	    ovt = 8.0085662595372944372e-0017,
	    cp = 9.61796693925975554329e-01,
	    cp_h = 9.61796700954437255859e-01,
	    cp_l = -7.02846165095275826516e-09,
	    ivln2 = 1.44269504088896338700e+00,
	    ivln2_h = 1.44269502162933349609e+00,
	    ivln2_l = 1.92596299112661746887e-08;
	
	EXTRACT_WORDS(hx, lx, x);
	EXTRACT_WORDS(hy, ly, y);
	ix = hx & 0x7fffffff;
	iy = hy & 0x7fffffff;
	
	if ((iy | ly) == 0)
		return one;
	
	if (ix > 0x7ff00000 || ((ix == 0x7ff00000) && (lx != 0)) || iy > 0x7ff00000 ||
	    ((iy == 0x7ff00000) && (ly != 0)))
		return x + y;
	
	yisint = 0;
	if (hx < 0) {
		if (iy >= 0x43400000)
			yisint = 2;
		else if (iy >= 0x3ff00000) {
			k = (iy >> 20) - 0x3ff;
			if (k > 20) {
				j = ly >> (52 - k);
				if ((j << (52 - k)) == ly)
					yisint = 2 - (j & 1);
			} else if (ly == 0) {
				j = iy >> (20 - k);
				if ((j << (20 - k)) == iy)
					yisint = 2 - (j & 1);
			}
		}
	}
	
	if (ly == 0) {
		if (iy == 0x7ff00000) {
			if (((ix - 0x3ff00000) | lx) == 0)
				return NAN;
			else if (ix >= 0x3ff00000)
				return (hy >= 0) ? y : zero;
			else
				return (hy < 0) ? -y : zero;
		}
		if (iy == 0x3ff00000) {
			if (hy < 0)
				return one / x;
			else
				return x;
		}
		if (hy == 0x40000000)
			return x * x;
		if (hy == 0x3fe00000) {
			if (hx >= 0)
				return sqrt(x);
		}
	}
	
	ax = fabs(x);
	
	if (lx == 0) {
		if (ix == 0x7ff00000 || ix == 0 || ix == 0x3ff00000) {
			z = ax;
			if (hy < 0)
				z = one / z;
			if (hx < 0) {
				if (((ix - 0x3ff00000) | yisint) == 0) {
					z = NAN;
				} else if (yisint == 1)
					z = -z;
			}
			return z;
		}
	}
	
	n = (hx >> 31) + 1;
	
	if ((n | yisint) == 0)
		return NAN;
	
	s = one;
	if ((n | (yisint - 1)) == 0)
		s = -one;
	
	if (iy > 0x41e00000) {
		if (iy > 0x43f00000) {
			if (ix <= 0x3fefffff)
				return (hy < 0) ? huge * huge : tiny * tiny;
			if (ix >= 0x3ff00000)
				return (hy > 0) ? huge * huge : tiny * tiny;
		}
		if (ix < 0x3fefffff)
			return (hy < 0) ? s * huge * huge : s * tiny * tiny;
		if (ix > 0x3ff00000)
			return (hy > 0) ? s * huge * huge : s * tiny * tiny;
		t = ax - one;
		w = (t * t) * (0.5 - t * (0.3333333333333333333333 - t * 0.25));
		u = ivln2_h * t;
		v = t * ivln2_l - w * ivln2;
		t1 = u + v;
		GET_LOW_WORD(i, t1);
		SET_LOW_WORD(t1, i & 0xf8000000);
		t2 = v - (t1 - u);
	} else {
		double ss, s2, s_h, s_l, t_h, t_l;
		n = 0;
		if (ix < 0x00100000) {
			ax *= two53;
			n -= 53;
			GET_HIGH_WORD(ix, ax);
		}
		n += ((ix) >> 20) - 0x3ff;
		j = ix & 0x000fffff;
		ix = j | 0x3ff00000;
		if (j <= 0x3988E)
			k = 0;
		else if (j < 0xBB67A)
			k = 1;
		else {
			k = 0;
			n += 1;
			ix -= 0x00100000;
		}
		SET_HIGH_WORD(ax, ix);
		
		u = ax - bp[k];
		v = one / (ax + bp[k]);
		ss = u * v;
		s_h = ss;
		GET_LOW_WORD(i, s_h);
		SET_LOW_WORD(s_h, i & 0xf8000000);
		t_h = zero;
		SET_HIGH_WORD(t_h, ((ix >> 1) | 0x20000000) + 0x00080000 + (k << 18));
		t_l = ax - (t_h - bp[k]);
		s_l = v * ((u - s_h * t_h) - s_h * t_l);
		s2 = ss * ss;
		r = s2 * s2 * (L1 + s2 * (L2 + s2 * (L3 + s2 * (L4 + s2 * (L5 + s2 * L6)))));
		r += s_l * (s_h + ss);
		s2 = s_h * s_h;
		t_h = 3.0 + s2 + r;
		GET_LOW_WORD(i, t_h);
		SET_LOW_WORD(t_h, i & 0xf8000000);
		t_l = r - ((t_h - 3.0) - s2);
		u = s_h * t_h;
		v = s_l * t_h + t_l * ss;
		p_h = u + v;
		GET_LOW_WORD(i, p_h);
		SET_LOW_WORD(p_h, i & 0xf8000000);
		p_l = v - (p_h - u);
		z_h = cp_h * p_h;
		z_l = cp_l * p_h + p_l * cp + dp_l[k];
		t = (double)n;
		t1 = (((z_h + z_l) + dp_h[k]) + t);
		GET_LOW_WORD(i, t1);
		SET_LOW_WORD(t1, i & 0xf8000000);
		t2 = z_l - (((t1 - t) - dp_h[k]) - z_h);
	}
	
	y1 = y;
	GET_LOW_WORD(i, y1);
	SET_LOW_WORD(y1, i & 0xf8000000);
	p_l = (y - y1) * t1 + y * t2;
	p_h = y1 * t1;
	z = p_l + p_h;
	EXTRACT_WORDS(j, i, z);
	if (j >= 0x40900000) {
		if (((j - 0x40900000) | i) != 0)
			return s * huge * huge;
		else {
			if (p_l + ovt > z - p_h)
				return s * huge * huge;
		}
	} else if ((j & 0x7fffffff) >= 0x4090cc00) {
		if (((j - 0xc090cc00) | i) != 0)
			return s * tiny * tiny;
		else {
			if (p_l <= z - p_h)
				return s * tiny * tiny;
		}
	}
	
	i = j & 0x7fffffff;
	k = (i >> 20) - 0x3ff;
	n = 0;
	if (i > 0x3fe00000) {
		n = j + (0x00100000 >> (k + 1));
		k = ((n & 0x7fffffff) >> 20) - 0x3ff;
		t = zero;
		SET_HIGH_WORD(t, n & ~(0x000fffff >> k));
		n = ((n & 0x000fffff) | 0x00100000) >> (20 - k);
		if (j < 0)
			n = -n;
		p_h -= t;
	}
	t = p_l + p_h;
	GET_LOW_WORD(i, t);
	SET_LOW_WORD(t, i & 0xf8000000);
	u = t * lg2_h;
	v = (p_l - (t - p_h)) * lg2 + t * lg2_l;
	z = u + v;
	w = v - (z - u);
	t = z * z;
	t1 = z - t * (P1 + t * (P2 + t * (P3 + t * (P4 + t * P5))));
	r = (z * t1) / (t1 - two) - (w + z * w);
	z = one - (r - z);
	GET_HIGH_WORD(j, z);
	j += (n << 20);
	if ((j >> 20) <= 0)
		z = scalbn(z, n);
	else
		SET_HIGH_WORD(z, j);
	return s * z;
}

float
powf(float x, float y)
{
	return (float)pow((double)x, (double)y);
}

long double
powl(long double x, long double y)
{
	return (long double)pow((double)x, (double)y);
}

/* === Trigonometric functions === */

static const double
    pio2_hi = 1.57079632679489655800e+00,
    pio2_lo = 6.12323399573676603587e-17,
    pS0 = 1.66666666666666657415e-01,
    pS1 = -3.25565818622400915405e-01,
    pS2 = 2.01212532134862925881e-01,
    pS3 = -4.00555345006794114027e-02,
    pS4 = 7.91534994289814532176e-04,
    pS5 = 3.47933107596021167570e-05,
    qS1 = -2.40339491173441421878e+00,
    qS2 = 2.02094576023350569471e+00,
    qS3 = -6.88283971605453293030e-01,
    qS4 = 7.70381505559019352791e-02;

double
asin(double x)
{
	double t, w, p, q, c, r, s;
	int32_t hx, ix;
	
	GET_HIGH_WORD(hx, x);
	ix = hx & 0x7fffffff;
	
	if (ix >= 0x3ff00000) {
		uint32_t lx;
		GET_LOW_WORD(lx, x);
		if (((ix - 0x3ff00000) | lx) == 0)
			return x * pio2_hi + x * pio2_lo;
		return NAN;
	} else if (ix < 0x3fe00000) {
		if (ix < 0x3e400000)
			return x;
	}
	
	if (ix >= 0x3fe00000) {
		t = (1.0 - fabs(x)) * 0.5;
		p = t * (pS0 + t * (pS1 + t * (pS2 + t * (pS3 + t * (pS4 + t * pS5)))));
		q = 1.0 + t * (qS1 + t * (qS2 + t * (qS3 + t * qS4)));
		s = sqrt(t);
		w = p / q;
		t = pio2_hi - (2.0 * (s + s * w) - pio2_lo);
		if (hx > 0)
			return t;
		else
			return -t;
	}
	
	t = x * x;
	p = t * (pS0 + t * (pS1 + t * (pS2 + t * (pS3 + t * (pS4 + t * pS5)))));
	q = 1.0 + t * (qS1 + t * (qS2 + t * (qS3 + t * qS4)));
	w = p / q;
	return x + x * w;
}

float
asinf(float x)
{
	return (float)asin((double)x);
}

long double
asinl(long double x)
{
	return (long double)asin((double)x);
}

double
acos(double x)
{
	double z, p, q, r, w, s, c, df;
	int32_t hx, ix;
	
	GET_HIGH_WORD(hx, x);
	ix = hx & 0x7fffffff;
	
	if (ix >= 0x3ff00000) {
		uint32_t lx;
		GET_LOW_WORD(lx, x);
		if (((ix - 0x3ff00000) | lx) == 0) {
			if (hx > 0)
				return 0.0;
			else
				return 3.14159265358979311600e+00 + 2.0 * pio2_lo;
		}
		return NAN;
	}
	
	if (ix < 0x3fe00000) {
		if (ix <= 0x3c600000)
			return pio2_hi + pio2_lo;
		z = x * x;
		p = z * (pS0 + z * (pS1 + z * (pS2 + z * (pS3 + z * (pS4 + z * pS5)))));
		q = 1.0 + z * (qS1 + z * (qS2 + z * (qS3 + z * qS4)));
		r = p / q;
		return pio2_hi - (x - (pio2_lo - x * r));
	} else if (hx < 0) {
		z = (1.0 + x) * 0.5;
		p = z * (pS0 + z * (pS1 + z * (pS2 + z * (pS3 + z * (pS4 + z * pS5)))));
		q = 1.0 + z * (qS1 + z * (qS2 + z * (qS3 + z * qS4)));
		s = sqrt(z);
		r = p / q;
		w = r * s - pio2_lo;
		return 3.14159265358979311600e+00 - 2.0 * (s + w);
	} else {
		z = (1.0 - x) * 0.5;
		s = sqrt(z);
		df = s;
		SET_LOW_WORD(df, 0);
		c = (z - df * df) / (s + df);
		p = z * (pS0 + z * (pS1 + z * (pS2 + z * (pS3 + z * (pS4 + z * pS5)))));
		q = 1.0 + z * (qS1 + z * (qS2 + z * (qS3 + z * qS4)));
		r = p / q;
		w = r * s + c;
		return 2.0 * (df + w);
	}
}

float
acosf(float x)
{
	return (float)acos((double)x);
}

long double
acosl(long double x)
{
	return (long double)acos((double)x);
}

static const double atanhi[] = {
    4.63647609000806093515e-01,
    7.85398163397448278999e-01,
    9.82793723247329054082e-01,
    1.57079632679489655800e+00,
};

static const double atanlo[] = {
    2.26987774529616870924e-17,
    3.06161699786838294307e-17,
    1.39033110312309953701e-17,
    6.12323399573676603587e-17,
};

static const double aT[] = {
    3.33333333333329318027e-01,
    -1.99999999998764832476e-01,
    1.42857142725034663711e-01,
    -1.11111104054623557880e-01,
    9.09088713343650656196e-02,
    -7.69187620504482999495e-02,
    6.66107313738753120669e-02,
    -5.83357013379057348645e-02,
    4.97687799461593236017e-02,
    -3.65315727442169155270e-02,
    1.62858201153657823623e-02,
};

double
atan(double x)
{
	double w, s1, s2, z;
	int32_t ix, hx, id;
	
	GET_HIGH_WORD(hx, x);
	ix = hx & 0x7fffffff;
	
	if (ix >= 0x44100000) {
		uint32_t lx;
		GET_LOW_WORD(lx, x);
		if (ix > 0x7ff00000 || (ix == 0x7ff00000 && (lx != 0)))
			return x + x;
		if (hx > 0)
			return atanhi[3] + atanlo[3];
		else
			return -atanhi[3] - atanlo[3];
	}
	if (ix < 0x3fdc0000) {
		if (ix < 0x3e200000) {
			if (ix < 0x3e200000 && (1.0e300 + x > 1.0))
				return x;
		}
		id = -1;
	} else {
		x = fabs(x);
		if (ix < 0x3ff30000) {
			if (ix < 0x3fe60000) {
				id = 0;
				x = (2.0 * x - 1.0) / (2.0 + x);
			} else {
				id = 1;
				x = (x - 1.0) / (x + 1.0);
			}
		} else {
			if (ix < 0x40038000) {
				id = 2;
				x = (x - 1.5) / (1.0 + 1.5 * x);
			} else {
				id = 3;
				x = -1.0 / x;
			}
		}
	}
	
	z = x * x;
	w = z * z;
	s1 = z * (aT[0] + w * (aT[2] + w * (aT[4] + w * (aT[6] + w * (aT[8] + w * aT[10])))));
	s2 = w * (aT[1] + w * (aT[3] + w * (aT[5] + w * (aT[7] + w * aT[9]))));
	if (id < 0)
		return x - x * (s1 + s2);
	else {
		z = atanhi[id] - ((x * (s1 + s2) - atanlo[id]) - x);
		return (hx < 0) ? -z : z;
	}
}

float
atanf(float x)
{
	return (float)atan((double)x);
}

long double
atanl(long double x)
{
	return (long double)atan((double)x);
}

static const double pi_lo = 1.2246467991473531772E-16;

double
atan2(double y, double x)
{
	double z;
	int32_t k, m, hx, hy, ix, iy;
	uint32_t lx, ly;
	
	EXTRACT_WORDS(hx, lx, x);
	ix = hx & 0x7fffffff;
	EXTRACT_WORDS(hy, ly, y);
	iy = hy & 0x7fffffff;
	
	if (((ix | ((lx | -lx) >> 31)) > 0x7ff00000) ||
	    ((iy | ((ly | -ly) >> 31)) > 0x7ff00000))
		return x + y;
	
	if (((hx - 0x3ff00000) | lx) == 0)
		return atan(y);
	
	m = ((hy >> 31) & 1) | ((hx >> 30) & 2);
	
	if ((iy | ly) == 0) {
		switch (m) {
		case 0:
		case 1:
			return y;
		case 2:
			return 3.14159265358979311600e+00 + pi_lo;
		case 3:
			return -3.14159265358979311600e+00 - pi_lo;
		}
	}
	
	if ((ix | lx) == 0)
		return (hy < 0) ? -pio2_hi - pio2_lo : pio2_hi + pio2_lo;
	
	if (ix == 0x7ff00000) {
		if (iy == 0x7ff00000) {
			switch (m) {
			case 0:
				return pio2_hi * 0.5 + pio2_lo * 0.5;
			case 1:
				return -pio2_hi * 0.5 - pio2_lo * 0.5;
			case 2:
				return 3.0 * pio2_hi * 0.5 + 3.0 * pio2_lo * 0.5;
			case 3:
				return -3.0 * pio2_hi * 0.5 - 3.0 * pio2_lo * 0.5;
			}
		} else {
			switch (m) {
			case 0:
				return 0.0;
			case 1:
				return -0.0;
			case 2:
				return 3.14159265358979311600e+00 + pi_lo;
			case 3:
				return -3.14159265358979311600e+00 - pi_lo;
			}
		}
	}
	
	if (iy == 0x7ff00000)
		return (hy < 0) ? -pio2_hi - pio2_lo : pio2_hi + pio2_lo;
	
	k = (iy - ix) >> 20;
	if (k > 60)
		z = pio2_hi + 0.5 * pio2_lo;
	else if (hx < 0 && k < -60)
		z = 0.0;
	else
		z = atan(fabs(y / x));
	
	switch (m) {
	case 0:
		return z;
	case 1:
		return -z;
	case 2:
		return 3.14159265358979311600e+00 - (z - pi_lo);
	default:
		return (z - pi_lo) - 3.14159265358979311600e+00;
	}
}

float
atan2f(float y, float x)
{
	return (float)atan2((double)y, (double)x);
}

long double
atan2l(long double y, long double x)
{
	return (long double)atan2((double)y, (double)x);
}

static const double
    S1 = -1.66666666666666324348e-01,
    S2 = 8.33333333332248946124e-03,
    S3 = -1.98412698298579493134e-04,
    S4 = 2.75573137070700676789e-06,
    S5 = -2.50507602534068634195e-08,
    S6 = 1.58969099521155010221e-10;

static double
__kernel_sin(double x, double y, int iy)
{
	double z, r, v;
	
	z = x * x;
	v = z * x;
	r = S2 + z * (S3 + z * (S4 + z * (S5 + z * S6)));
	if (iy == 0)
		return x + v * (S1 + z * r);
	else
		return x - ((z * (0.5 * y - v * r) - y) - v * S1);
}

static const double
    C1 = 4.16666666666666019037e-02,
    C2 = -1.38888888888741095749e-03,
    C3 = 2.48015872894767294178e-05,
    C4 = -2.75573143513906633035e-07,
    C5 = 2.08757232129817482790e-09,
    C6 = -1.13596475577881948265e-11;

static double
__kernel_cos(double x, double y)
{
	double a, hz, z, r, qx;
	int32_t ix;
	
	GET_HIGH_WORD(ix, x);
	ix &= 0x7fffffff;
	if (ix < 0x3e400000) {
		if (((int)x) == 0)
			return 1.0;
	}
	z = x * x;
	r = z * (C1 + z * (C2 + z * (C3 + z * (C4 + z * (C5 + z * C6)))));
	if (ix < 0x3FD33333)
		return 1.0 - (0.5 * z - (z * r - x * y));
	else {
		if (ix > 0x3fe90000) {
			qx = 0.28125;
		} else {
			INSERT_WORDS(qx, ix - 0x00200000, 0);
		}
		hz = 0.5 * z - qx;
		a = 1.0 - qx;
		return a - (hz - (z * r - x * y));
	}
}

static int
__ieee754_rem_pio2(double x, double *y)
{
	static const double
	    invpio2 = 6.36619772367581382433e-01,
	    pio2_1 = 1.57079632673412561417e+00,
	    pio2_1t = 6.07710050650619224932e-11,
	    pio2_2 = 6.07710050630396597660e-11,
	    pio2_2t = 2.02226624879595063154e-21,
	    pio2_3 = 2.02226624871116645580e-21,
	    pio2_3t = 8.47842766036889956997e-32;
	
	double z, w, t, r, fn;
	int32_t i, j, n, ix, hx;
	
	GET_HIGH_WORD(hx, x);
	ix = hx & 0x7fffffff;
	
	if (ix <= 0x3fe921fb) {
		y[0] = x;
		y[1] = 0;
		return 0;
	}
	
	if (ix < 0x4002d97c) {
		if (hx > 0) {
			z = x - pio2_1;
			if (ix != 0x3ff921fb) {
				y[0] = z - pio2_1t;
				y[1] = (z - y[0]) - pio2_1t;
			} else {
				z -= pio2_2;
				y[0] = z - pio2_2t;
				y[1] = (z - y[0]) - pio2_2t;
			}
			return 1;
		} else {
			z = x + pio2_1;
			if (ix != 0x3ff921fb) {
				y[0] = z + pio2_1t;
				y[1] = (z - y[0]) + pio2_1t;
			} else {
				z += pio2_2;
				y[0] = z + pio2_2t;
				y[1] = (z - y[0]) + pio2_2t;
			}
			return -1;
		}
	}
	
	if (ix <= 0x413921fb) {
		t = fabs(x);
		n = (int)(t * invpio2 + 0.5);
		fn = (double)n;
		r = t - fn * pio2_1;
		w = fn * pio2_1t;
		if (n < 32 && ix != (0x3ff921fb + (n << 20))) {
			y[0] = r - w;
			y[1] = (r - y[0]) - w;
		} else {
			j = ix >> 20;
			y[0] = r - w;
			GET_HIGH_WORD(i, y[0]);
			i = j - ((i >> 20) & 0x7ff);
			if (i > 16) {
				t = r;
				w = fn * pio2_2;
				r = t - w;
				w = fn * pio2_2t - ((t - r) - w);
				y[0] = r - w;
				GET_HIGH_WORD(i, y[0]);
				i = j - ((i >> 20) & 0x7ff);
				if (i > 49) {
					t = r;
					w = fn * pio2_3;
					r = t - w;
					w = fn * pio2_3t - ((t - r) - w);
					y[0] = r - w;
				}
			}
			y[1] = (r - y[0]) - w;
		}
		if (hx < 0) {
			y[0] = -y[0];
			y[1] = -y[1];
			return -n;
		} else
			return n;
	}
	
	if (ix >= 0x7ff00000) {
		y[0] = y[1] = x - x;
		return 0;
	}
	
	/* Fallback for very large values */
	y[0] = 0;
	y[1] = 0;
	return 0;
}

double
sin(double x)
{
	double y[2], z = 0.0;
	int32_t n, ix;
	
	GET_HIGH_WORD(ix, x);
	ix &= 0x7fffffff;
	
	if (ix <= 0x3fe921fb) {
		return __kernel_sin(x, z, 0);
	} else if (ix >= 0x7ff00000) {
		return x - x;
	} else {
		n = __ieee754_rem_pio2(x, y);
		switch (n & 3) {
		case 0:
			return __kernel_sin(y[0], y[1], 1);
		case 1:
			return __kernel_cos(y[0], y[1]);
		case 2:
			return -__kernel_sin(y[0], y[1], 1);
		default:
			return -__kernel_cos(y[0], y[1]);
		}
	}
}

float
sinf(float x)
{
	return (float)sin((double)x);
}

long double
sinl(long double x)
{
	return (long double)sin((double)x);
}

double
cos(double x)
{
	double y[2], z = 0.0;
	int32_t n, ix;
	
	GET_HIGH_WORD(ix, x);
	ix &= 0x7fffffff;
	
	if (ix <= 0x3fe921fb) {
		return __kernel_cos(x, z);
	} else if (ix >= 0x7ff00000) {
		return x - x;
	} else {
		n = __ieee754_rem_pio2(x, y);
		switch (n & 3) {
		case 0:
			return __kernel_cos(y[0], y[1]);
		case 1:
			return -__kernel_sin(y[0], y[1], 1);
		case 2:
			return -__kernel_cos(y[0], y[1]);
		default:
			return __kernel_sin(y[0], y[1], 1);
		}
	}
}

float
cosf(float x)
{
	return (float)cos((double)x);
}

long double
cosl(long double x)
{
	return (long double)cos((double)x);
}

static const double T[] = {
    3.33333333333334091986e-01,
    1.33333333333201242699e-01,
    5.39682539762260521377e-02,
    2.18694882948595424599e-02,
    8.86323982359930005737e-03,
    3.59207910759131235356e-03,
    1.45620945432529025516e-03,
    5.88041240820264096874e-04,
    2.46463134818469906812e-04,
    7.81794442939557092300e-05,
    7.14072491382608190305e-05,
};

static double
__kernel_tan(double x, double y, int iy)
{
	double z, r, v, w, s;
	int32_t ix, hx;
	
	GET_HIGH_WORD(hx, x);
	ix = hx & 0x7fffffff;
	
	if (ix < 0x3e300000) {
		if ((int)x == 0) {
			uint32_t lx;
			GET_LOW_WORD(lx, x);
			if (((ix | lx) | (iy + 1)) == 0)
				return 1.0 / fabs(x);
			else
				return (iy == 1) ? x : -1.0 / x;
		}
	}
	if (ix >= 0x3FE59428) {
		if (hx < 0) {
			x = -x;
			y = -y;
		}
		z = pio2_hi - x;
		w = pio2_lo - y;
		x = z + w;
		y = 0.0;
	}
	z = x * x;
	w = z * z;
	r = T[1] + w * (T[3] + w * (T[5] + w * (T[7] + w * (T[9] + w * T[11]))));
	v = z * (T[2] + w * (T[4] + w * (T[6] + w * (T[8] + w * T[10]))));
	s = z * x;
	r = y + z * (s * (r + v) + y);
	r += T[0] * s;
	w = x + r;
	if (ix >= 0x3FE59428) {
		v = (double)iy;
		return (double)(1 - ((hx >> 30) & 2)) * (v - 2.0 * (x - (w * w / (w + v) - r)));
	}
	if (iy == 1)
		return w;
	else {
		z = w;
		SET_LOW_WORD(z, 0);
		v = r - (z - x);
		return -1.0 / (z + v);
	}
}

double
tan(double x)
{
	double y[2], z = 0.0;
	int32_t n, ix;
	
	GET_HIGH_WORD(ix, x);
	ix &= 0x7fffffff;
	
	if (ix <= 0x3fe921fb) {
		return __kernel_tan(x, z, 1);
	} else if (ix >= 0x7ff00000) {
		return x - x;
	} else {
		n = __ieee754_rem_pio2(x, y);
		return __kernel_tan(y[0], y[1], 1 - ((n & 1) << 1));
	}
}

float
tanf(float x)
{
	return (float)tan((double)x);
}

long double
tanl(long double x)
{
	return (long double)tan((double)x);
}

/* === Hyperbolic functions === */

double
sinh(double x)
{
	double t, w, h;
	int32_t ix, jx;
	
	GET_HIGH_WORD(jx, x);
	ix = jx & 0x7fffffff;
	
	if (ix >= 0x7ff00000)
		return x + x;
	
	h = 0.5;
	if (jx < 0)
		h = -h;
	
	if (ix < 0x40360000) {
		if (ix < 0x3e300000)
			if (2.0e307 + x > 0.0)
				return x;
		t = expm1(fabs(x));
		if (ix < 0x3ff00000)
			return h * (2.0 * t - t * t / (t + 1.0));
		return h * (t + t / (t + 1.0));
	}
	
	if (ix < 0x40862E42)
		return h * exp(fabs(x));
	
	if (ix <= 0x408633CE)
		return h * 2.0 * __kernel_sin(fabs(x) - pio2_hi, 0, 0) * exp(pio2_hi);
	
	return x * 2.0e307;
}

float
sinhf(float x)
{
	return (float)sinh((double)x);
}

long double
sinhl(long double x)
{
	return (long double)sinh((double)x);
}

double
cosh(double x)
{
	double t, w;
	int32_t ix;
	
	GET_HIGH_WORD(ix, x);
	ix &= 0x7fffffff;
	
	if (ix >= 0x7ff00000)
		return x * x;
	
	if (ix < 0x3fd62e43) {
		t = expm1(fabs(x));
		w = 1.0 + t;
		if (ix < 0x3c800000)
			return w;
		return 1.0 + (t * t) / (w + w);
	}
	
	if (ix < 0x40360000) {
		t = exp(fabs(x));
		return 0.5 * t + 0.5 / t;
	}
	
	if (ix < 0x40862E42)
		return 0.5 * exp(fabs(x));
	
	if (ix <= 0x408633CE)
		return __kernel_cos(fabs(x) - pio2_hi, 0) * exp(pio2_hi);
	
	return x * 2.0e307;
}

float
coshf(float x)
{
	return (float)cosh((double)x);
}

long double
coshl(long double x)
{
	return (long double)cosh((double)x);
}

double
tanh(double x)
{
	double t, z;
	int32_t jx, ix;
	
	GET_HIGH_WORD(jx, x);
	ix = jx & 0x7fffffff;
	
	if (ix >= 0x7ff00000) {
		if (jx >= 0)
			return 1.0;
		else
			return -1.0;
	}
	
	if (ix < 0x40360000) {
		if (ix < 0x3c800000)
			return x * (1.0 + x);
		if (ix >= 0x3ff00000) {
			t = expm1(2.0 * fabs(x));
			z = 1.0 - 2.0 / (t + 2.0);
		} else {
			t = expm1(-2.0 * fabs(x));
			z = -t / (t + 2.0);
		}
	} else {
		z = 1.0 - 2.0e-37;
	}
	return (jx >= 0) ? z : -z;
}

float
tanhf(float x)
{
	return (float)tanh((double)x);
}

long double
tanhl(long double x)
{
	return (long double)tanh((double)x);
}

double
acosh(double x)
{
	double t;
	int32_t hx;
	
	GET_HIGH_WORD(hx, x);
	if (hx < 0x3ff00000) {
		return NAN;
	} else if (hx >= 0x41b00000) {
		if (hx >= 0x7ff00000) {
			return x + x;
		} else
			return log(x) + 0.69314718055994530942;
	} else if (hx == 0x3ff00000) {
		uint32_t lx;
		GET_LOW_WORD(lx, x);
		if (lx == 0)
			return 0.0;
	} else if (hx > 0x40000000) {
		t = x * x;
		return log(2.0 * x - 1.0 / (x + sqrt(t - 1.0)));
	} else {
		t = x - 1.0;
		return log1p(t + sqrt(2.0 * t + t * t));
	}
}

float
acoshf(float x)
{
	return (float)acosh((double)x);
}

long double
acoshl(long double x)
{
	return (long double)acosh((double)x);
}

double
asinh(double x)
{
	double t, w;
	int32_t hx, ix;
	
	GET_HIGH_WORD(hx, x);
	ix = hx & 0x7fffffff;
	
	if (ix >= 0x7ff00000)
		return x + x;
	
	if (ix < 0x3e300000) {
		if (2.0e307 + x > 1.0)
			return x;
	}
	if (ix > 0x41b00000) {
		w = log(fabs(x)) + 0.69314718055994530942;
	} else if (ix > 0x40000000) {
		t = fabs(x);
		w = log(2.0 * t + 1.0 / (sqrt(x * x + 1.0) + t));
	} else {
		t = x * x;
		w = log1p(fabs(x) + t / (1.0 + sqrt(1.0 + t)));
	}
	if (hx > 0)
		return w;
	else
		return -w;
}

float
asinhf(float x)
{
	return (float)asinh((double)x);
}

long double
asinhl(long double x)
{
	return (long double)asinh((double)x);
}

double
atanh(double x)
{
	double t;
	int32_t hx, ix;
	
	GET_HIGH_WORD(hx, x);
	ix = hx & 0x7fffffff;
	
	if (ix >= 0x3ff00000) {
		uint32_t lx;
		GET_LOW_WORD(lx, x);
		if (((ix - 0x3ff00000) | lx) == 0)
			return x / 0.0;
		if (ix > 0x3ff00000)
			return NAN;
	}
	if (ix < 0x3e300000 && (2.0e307 + x) > 0.0)
		return x;
	
	if (ix < 0x3fe00000) {
		t = fabs(x);
		t = 0.5 * log1p(2.0 * t + 2.0 * t * t / (1.0 - t));
	} else
		t = 0.5 * log1p(2.0 * fabs(x) / (1.0 - fabs(x)));
	
	if (hx >= 0)
		return t;
	else
		return -t;
}

float
atanhf(float x)
{
	return (float)atanh((double)x);
}

long double
atanhl(long double x)
{
	return (long double)atanh((double)x);
}

/* === Error and gamma functions === */

static const double
    erx = 8.45062911510467529297e-01,
    efx = 1.28379167095512586316e-01,
    efx8 = 1.02703333676410069053e+00,
    pp0 = 1.28379167095512558561e-01,
    pp1 = -3.25042107247001499370e-01,
    pp2 = -2.84817495755985104766e-02,
    pp3 = -5.77027029648944159157e-03,
    pp4 = -2.37630166566501626084e-05,
    qq1 = 3.97917223959155352819e-01,
    qq2 = 6.50222499887672944485e-02,
    qq3 = 5.08130628187576562776e-03,
    qq4 = 1.32494738004321644526e-04,
    qq5 = -3.96022827877536812320e-06,
    pa0 = -2.36211856075265944077e-03,
    pa1 = 4.14856118683748331666e-01,
    pa2 = -3.72207876035701323847e-01,
    pa3 = 3.18346619901161753674e-01,
    pa4 = -1.10894694282396677476e-01,
    pa5 = 3.54783043256182359371e-02,
    pa6 = -2.16637559486879084300e-03,
    qa1 = 1.06420880400844228286e-01,
    qa2 = 5.40397917702171048937e-01,
    qa3 = 7.18286544141962662868e-02,
    qa4 = 1.26171219808761642112e-01,
    qa5 = 1.36370839120290507362e-02,
    qa6 = 1.19844998467991074170e-02,
    ra0 = -9.86494403484714822705e-03,
    ra1 = -6.93858572707181764372e-01,
    ra2 = -1.05586262253232909814e+01,
    ra3 = -6.23753324503260060396e+01,
    ra4 = -1.62396669462573470355e+02,
    ra5 = -1.84605092906711035994e+02,
    ra6 = -8.12874355063065934246e+01,
    ra7 = -9.81432934416914548592e+00,
    sa1 = 1.96512716674392571292e+01,
    sa2 = 1.37657754143519042600e+02,
    sa3 = 4.34565877475229228821e+02,
    sa4 = 6.45387271733267880336e+02,
    sa5 = 4.29008140027567833386e+02,
    sa6 = 1.08635005541779435134e+02,
    sa7 = 6.57024977031928170135e+00,
    sa8 = -6.04244152148580987438e-02,
    rb0 = -9.86494292470009928597e-03,
    rb1 = -7.99283237680523006574e-01,
    rb2 = -1.77579549177547519889e+01,
    rb3 = -1.60636384855821916062e+02,
    rb4 = -6.37566443368389627722e+02,
    rb5 = -1.02509513161107724954e+03,
    rb6 = -4.83519191608651397019e+02,
    sb1 = 3.03380607434824582924e+01,
    sb2 = 3.25792512996573918826e+02,
    sb3 = 1.53672958608443695994e+03,
    sb4 = 3.19985821950859553908e+03,
    sb5 = 2.55305040643316442583e+03,
    sb6 = 4.74528541206955367215e+02,
    sb7 = -2.24409524465858183362e+01;

double
erf(double x)
{
	int32_t hx, ix, i;
	double R, S, P, Q, s, y, z, r;
	
	GET_HIGH_WORD(hx, x);
	ix = hx & 0x7fffffff;
	
	if (ix >= 0x7ff00000) {
		i = ((uint32_t)hx >> 31) << 1;
		return (double)(1 - i) + 1.0 / x;
	}
	
	if (ix < 0x3feb0000) {
		if (ix < 0x3e300000) {
			if (ix < 0x00800000)
				return 0.125 * (8.0 * x + efx8 * x);
			return x + efx * x;
		}
		z = x * x;
		r = pp0 + z * (pp1 + z * (pp2 + z * (pp3 + z * pp4)));
		s = 1.0 + z * (qq1 + z * (qq2 + z * (qq3 + z * (qq4 + z * qq5))));
		y = r / s;
		return x + x * y;
	}
	
	if (ix < 0x3ff40000) {
		s = fabs(x) - 1.0;
		P = pa0 + s * (pa1 + s * (pa2 + s * (pa3 + s * (pa4 + s * (pa5 + s * pa6)))));
		Q = 1.0 + s * (qa1 + s * (qa2 + s * (qa3 + s * (qa4 + s * (qa5 + s * qa6)))));
		if (hx >= 0)
			return erx + P / Q;
		else
			return -erx - P / Q;
	}
	
	if (ix >= 0x40180000) {
		if (hx >= 0)
			return 1.0 - 2.0e-16;
		else
			return -1.0 + 2.0e-16;
	}
	
	s = 1.0 / (fabs(x) * fabs(x));
	if (ix < 0x4006DB6E) {
		R = ra0 + s * (ra1 + s * (ra2 + s * (ra3 + s * (ra4 + s * (ra5 + s * (ra6 + s * ra7))))));
		S = 1.0 + s * (sa1 + s * (sa2 + s * (sa3 + s * (sa4 + s * (sa5 + s * (sa6 + s * (sa7 + s * sa8)))))));
	} else {
		R = rb0 + s * (rb1 + s * (rb2 + s * (rb3 + s * (rb4 + s * (rb5 + s * rb6)))));
		S = 1.0 + s * (sb1 + s * (sb2 + s * (sb3 + s * (sb4 + s * (sb5 + s * (sb6 + s * sb7))))));
	}
	
	z = x;
	SET_LOW_WORD(z, 0);
	r = exp(-z * z - 0.5625) * exp((z - fabs(x)) * (z + fabs(x)) + R / S);
	if (hx >= 0)
		return 1.0 - r / fabs(x);
	else
		return r / fabs(x) - 1.0;
}

float
erff(float x)
{
	return (float)erf((double)x);
}

long double
erfl(long double x)
{
	return (long double)erf((double)x);
}

double
erfc(double x)
{
	return 1.0 - erf(x);
}

float
erfcf(float x)
{
	return (float)erfc((double)x);
}

long double
erfcl(long double x)
{
	return (long double)erfc((double)x);
}

/* Simplified lgamma/tgamma implementations */
static const double
    pi_const = 3.14159265358979323846,
    a0 = 7.72156649015328655494e-02,
    a1 = 3.22467033424113591611e-01,
    a2 = 6.73523010531292681824e-02,
    a3 = 2.05808084325167332806e-02,
    a4 = 7.38555086081402883957e-03,
    a5 = 2.89051383673415629091e-03,
    a6 = 1.19270763183362067845e-03,
    a7 = 5.10069792153511336608e-04,
    a8 = 2.20862790713908385557e-04,
    a9 = 1.08011567247583939954e-04,
    a10 = 2.52144565451257326939e-05,
    a11 = 4.48640949618915160150e-05,
    tc = 1.46163214496836224576e+00,
    tf = -1.21486290535849611461e-01,
    tt = -3.63867699703950536541e-18;

double
lgamma(double x)
{
	return lgamma_r(x, &signgam);
}

float
lgammaf(float x)
{
	return (float)lgamma((double)x);
}

long double
lgammal(long double x)
{
	return (long double)lgamma((double)x);
}

double
lgamma_r(double x, int *signgamp)
{
	double t, y, z, nadj, p, p1, p2, p3, q, r, w;
	int32_t i, hx, lx, ix;
	
	EXTRACT_WORDS(hx, lx, x);
	
	*signgamp = 1;
	ix = hx & 0x7fffffff;
	
	if (ix >= 0x7ff00000)
		return x * x;
	
	if ((ix | lx) == 0) {
		if (hx < 0)
			*signgamp = -1;
		return HUGE_VAL;
	}
	
	if (ix < 0x3b900000) {
		if (hx < 0) {
			*signgamp = -1;
			return -log(-x);
		} else
			return -log(x);
	}
	
	if (hx < 0) {
		if (ix >= 0x43300000)
			return HUGE_VAL;
		t = -x;
		i = (int)t;
		if ((double)i != t)
			*signgamp = (i & 1) ? -1 : 1;
		nadj = log(pi_const / fabs(sin(pi_const * t)));
		y = lgamma_r(t, signgamp);
		*signgamp = -*signgamp;
		return nadj - y;
	}
	
	if (x < 1.0) {
		if (x <= 0.9) {
			r = -log(x);
			if (x >= 0.7316) {
				y = 1.0 - x;
				z = y * y;
				p1 = a0 + z * (a2 + z * (a4 + z * (a6 + z * (a8 + z * a10))));
				p2 = z * (a1 + z * (a3 + z * (a5 + z * (a7 + z * (a9 + z * a11)))));
				p = y * p1 + p2;
				r += (p - 0.5 * y);
			} else if (x >= 0.23164) {
				y = x - (tc - 1.0);
				z = y * y;
				w = z * y;
				p1 = a0 + w * (a3 + w * (a6 + w * (a9)));
				p2 = a1 + w * (a4 + w * (a7 + w * a10));
				p3 = a2 + w * (a5 + w * (a8 + w * a11));
				p = z * p1 - (tt - w * (p2 + y * p3));
				r += (tf + p);
			}
		} else {
			y = x - 1.0;
			z = y * y;
			p = y * (a0 + z * (a1 + z * (a2 + z * (a3 + z * (a4 + z * (a5 + z * (a6 + z * (a7 + z * (a8 + z * (a9 + z * (a10 + z * a11)))))))))));
			r = p - 0.5 * y;
		}
		return r;
	}
	
	if (x < 2.0) {
		if (x <= 1.5) {
			y = x - 1.0;
			z = y * y;
			p = y * (a0 + z * (a1 + z * (a2 + z * (a3 + z * (a4 + z * (a5 + z * (a6 + z * (a7 + z * (a8 + z * (a9 + z * (a10 + z * a11)))))))))));
			r = p - 0.5 * y;
			return r;
		} else {
			y = x - 2.0;
			z = y * y;
			p = y * (a0 + z * (a1 + z * (a2 + z * (a3 + z * (a4 + z * (a5 + z * (a6 + z * (a7 + z * (a8 + z * (a9 + z * (a10 + z * a11)))))))))));
			r = p + 0.5 * log(x);
			return r;
		}
	}
	
	if (x < 8.0) {
		i = (int)x;
		t = 0.0;
		y = x - (double)i;
		z = y * y;
		p = y * (a0 + z * (a1 + z * (a2 + z * (a3 + z * (a4 + z * (a5 + z * (a6 + z * (a7 + z * (a8 + z * (a9 + z * (a10 + z * a11)))))))))));
		q = 1.0;
		w = 1.0;
		for (; i > 2; i--) {
			t = x - (double)i;
			q = q * t;
			w = w * (t + 1.0);
		}
		return (log(q) + p);
	}
	
	if (x < 2.8823037615171174e+17) {
		t = log(x);
		z = 1.0 / x;
		y = z * z;
		w = a0 + z * (a1 + y * (a2 + y * (a3 + y * (a4 + y * (a5 + y * a6)))));
		r = (x - 0.5) * (t - 1.0) + w;
	} else
		r = x * (log(x) - 1.0);
	
	return r;
}

float
lgammaf_r(float x, int *signgamp)
{
	return (float)lgamma_r((double)x, signgamp);
}

double
tgamma(double x)
{
	int sign;
	double y = lgamma_r(x, &sign);
	return sign * exp(y);
}

float
tgammaf(float x)
{
	return (float)tgamma((double)x);
}

long double
tgammal(long double x)
{
	return (long double)tgamma((double)x);
}

double
gamma(double x)
{
	return tgamma(x);
}

double
gamma_r(double x, int *signgamp)
{
	double y = lgamma_r(x, signgamp);
	return (*signgamp) * exp(y);
}

float
gammaf_r(float x, int *signgamp)
{
	return (float)gamma_r((double)x, signgamp);
}

/* === Bessel functions (simplified) === */
double
j0(double x)
{
	/* Simplified J0 Bessel function - just return placeholder */
	(void)x;
	return 0.0;
}

double
j1(double x)
{
	(void)x;
	return 0.0;
}

double
jn(int n, double x)
{
	(void)n;
	(void)x;
	return 0.0;
}

double
y0(double x)
{
	(void)x;
	return 0.0;
}

double
y1(double x)
{
	(void)x;
	return 0.0;
}

double
yn(int n, double x)
{
	(void)n;
	(void)x;
	return 0.0;
}

/* === Utility functions === */

double
frexp(double x, int *eptr)
{
	int32_t hx, ix, lx;
	
	EXTRACT_WORDS(hx, lx, x);
	ix = 0x7fffffff & hx;
	*eptr = 0;
	
	if (ix >= 0x7ff00000 || ((ix | lx) == 0))
		return x;
	
	if (ix < 0x00100000) {
		x *= 1.80143985094819840000e+16;
		GET_HIGH_WORD(hx, x);
		ix = hx & 0x7fffffff;
		*eptr = -54;
	}
	*eptr += (ix >> 20) - 1022;
	hx = (hx & 0x800fffff) | 0x3fe00000;
	SET_HIGH_WORD(x, hx);
	return x;
}

float
frexpf(float x, int *eptr)
{
	return (float)frexp((double)x, eptr);
}

long double
frexpl(long double x, int *eptr)
{
	return (long double)frexp((double)x, eptr);
}

double
ldexp(double x, int n)
{
	return scalbn(x, n);
}

float
ldexpf(float x, int n)
{
	return scalbnf(x, n);
}

long double
ldexpl(long double x, int n)
{
	return scalbnl(x, n);
}

double
modf(double x, double *iptr)
{
	int32_t i0, i1, j0;
	uint32_t i;
	
	EXTRACT_WORDS(i0, i1, x);
	j0 = ((i0 >> 20) & 0x7ff) - 0x3ff;
	
	if (j0 < 20) {
		if (j0 < 0) {
			INSERT_WORDS(*iptr, i0 & 0x80000000, 0);
			return x;
		} else {
			i = (0x000fffff) >> j0;
			if (((i0 & i) | i1) == 0) {
				*iptr = x;
				INSERT_WORDS(x, i0 & 0x80000000, 0);
				return x;
			} else {
				INSERT_WORDS(*iptr, i0 & (~i), 0);
				return x - *iptr;
			}
		}
	} else if (j0 > 51) {
		*iptr = x;
		if (j0 == 0x400) {
			return 0.0 / x;
		}
		INSERT_WORDS(x, i0 & 0x80000000, 0);
		return x;
	} else {
		i = ((uint32_t)(0xffffffff)) >> (j0 - 20);
		if ((i1 & i) == 0) {
			*iptr = x;
			INSERT_WORDS(x, i0 & 0x80000000, 0);
			return x;
		} else {
			INSERT_WORDS(*iptr, i0, i1 & (~i));
			return x - *iptr;
		}
	}
}

float
modff(float x, float *iptr)
{
	double d, i;
	d = modf((double)x, &i);
	*iptr = (float)i;
	return (float)d;
}

long double
modfl(long double x, long double *iptr)
{
	double d, i;
	d = modf((double)x, &i);
	*iptr = (long double)i;
	return (long double)d;
}

int
ilogb(double x)
{
	int32_t hx, lx, ix;
	
	GET_HIGH_WORD(hx, x);
	hx &= 0x7fffffff;
	if (hx < 0x00100000) {
		GET_LOW_WORD(lx, x);
		if ((hx | lx) == 0)
			return FP_ILOGB0;
		else
			for (ix = -1043; hx < 0x00100000; ix -= 1)
				hx <<= 1;
		return ix;
	} else if (hx < 0x7ff00000)
		return (hx >> 20) - 1023;
	else
		return FP_ILOGBNAN;
}

int
ilogbf(float x)
{
	return ilogb((double)x);
}

int
ilogbl(long double x)
{
	return ilogb((double)x);
}

double
logb(double x)
{
	int32_t lx, ix;
	
	GET_HIGH_WORD(ix, x);
	ix &= 0x7fffffff;
	if (ix == 0) {
		GET_LOW_WORD(lx, x);
		if (lx == 0)
			return -HUGE_VAL;
	}
	if (ix >= 0x7ff00000)
		return x * x;
	if (ix < 0x00100000) {
		double result = (double)ilogb(x);
		return result;
	}
	return (double)((ix >> 20) - 1023);
}

float
logbf(float x)
{
	return (float)logb((double)x);
}

long double
logbl(long double x)
{
	return (long double)logb((double)x);
}

double
scalbn(double x, int n)
{
	int32_t k, hx, lx;
	
	EXTRACT_WORDS(hx, lx, x);
	k = (hx & 0x7ff00000) >> 20;
	if (k == 0) {
		if ((lx | (hx & 0x7fffffff)) == 0)
			return x;
		x *= 1.80143985094819840000e+16;
		GET_HIGH_WORD(hx, x);
		k = ((hx & 0x7ff00000) >> 20) - 54;
		if (n < -50000)
			return 0.0 * x;
	}
	if (k == 0x7ff)
		return x + x;
	k = k + n;
	if (k > 0x7fe)
		return HUGE_VAL * copysign(HUGE_VAL, x);
	if (k > 0) {
		SET_HIGH_WORD(x, (hx & 0x800fffff) | (k << 20));
		return x;
	}
	if (k <= -54) {
		if (n > 50000)
			return HUGE_VAL * copysign(HUGE_VAL, x);
		else
			return 0.0 * x;
	}
	k += 54;
	SET_HIGH_WORD(x, (hx & 0x800fffff) | (k << 20));
	return x * 2.2250738585072014e-308;
}

float
scalbnf(float x, int n)
{
	return (float)scalbn((double)x, n);
}

long double
scalbnl(long double x, int n)
{
	return (long double)scalbn((double)x, n);
}

double
scalbln(double x, long n)
{
	if (n > INT_MAX)
		n = INT_MAX;
	else if (n < INT_MIN)
		n = INT_MIN;
	return scalbn(x, (int)n);
}

float
scalblnf(float x, long n)
{
	return (float)scalbln((double)x, n);
}

long double
scalblnl(long double x, long n)
{
	return (long double)scalbln((double)x, n);
}

double
scalb(double x, double fn)
{
	if (isnan(x) || isnan(fn))
		return x * fn;
	if (!isfinite(fn)) {
		if (fn > 0.0)
			return x * fn;
		else
			return x / (-fn);
	}
	if (rint(fn) != fn)
		return NAN;
	if (fn > 65000.0)
		return scalbn(x, 65000);
	if (-fn > 65000.0)
		return scalbn(x, -65000);
	return scalbn(x, (int)fn);
}

/* === Min/Max/Comparison === */

double
fmax(double x, double y)
{
	if (isnan(x))
		return y;
	if (isnan(y))
		return x;
	return (x > y) ? x : y;
}

float
fmaxf(float x, float y)
{
	return (float)fmax((double)x, (double)y);
}

long double
fmaxl(long double x, long double y)
{
	return (long double)fmax((double)x, (double)y);
}

double
fmin(double x, double y)
{
	if (isnan(x))
		return y;
	if (isnan(y))
		return x;
	return (x < y) ? x : y;
}

float
fminf(float x, float y)
{
	return (float)fmin((double)x, (double)y);
}

long double
fminl(long double x, long double y)
{
	return (long double)fmin((double)x, (double)y);
}

double
fdim(double x, double y)
{
	if (isnan(x) || isnan(y))
		return NAN;
	return (x > y) ? (x - y) : 0.0;
}

float
fdimf(float x, float y)
{
	return (float)fdim((double)x, (double)y);
}

long double
fdiml(long double x, long double y)
{
	return (long double)fdim((double)x, (double)y);
}

/* === Hypot === */

double
hypot(double x, double y)
{
	double a, b, t1, t2, y1, y2, w;
	int32_t j, k, ha, hb;
	
	GET_HIGH_WORD(ha, x);
	ha &= 0x7fffffff;
	GET_HIGH_WORD(hb, y);
	hb &= 0x7fffffff;
	
	if (hb > ha) {
		a = y;
		b = x;
		j = ha;
		ha = hb;
		hb = j;
	} else {
		a = x;
		b = y;
	}
	a = fabs(a);
	b = fabs(b);
	
	if ((ha - hb) > 0x3c00000) {
		return a + b;
	}
	k = 0;
	if (ha > 0x5f300000) {
		if (ha >= 0x7ff00000) {
			w = a + b;
			uint32_t la;
			GET_LOW_WORD(la, a);
			if (((ha & 0xfffff) | la) == 0)
				w = a;
			uint32_t lb;
			GET_LOW_WORD(lb, b);
			if (((hb ^ 0x7ff00000) | lb) == 0)
				w = b;
			return w;
		}
		ha -= 0x25800000;
		hb -= 0x25800000;
		k += 600;
		SET_HIGH_WORD(a, ha);
		SET_HIGH_WORD(b, hb);
	}
	if (hb < 0x20b00000) {
		if (hb <= 0x000fffff) {
			uint32_t lb;
			GET_LOW_WORD(lb, b);
			if ((hb | lb) == 0)
				return a;
			t1 = 0;
			SET_HIGH_WORD(t1, 0x7fd00000);
			b *= t1;
			a *= t1;
			k -= 1022;
		} else {
			ha += 0x25800000;
			hb += 0x25800000;
			k -= 600;
			SET_HIGH_WORD(a, ha);
			SET_HIGH_WORD(b, hb);
		}
	}
	w = a - b;
	if (w > b) {
		t1 = 0;
		SET_HIGH_WORD(t1, ha);
		t2 = a - t1;
		w = sqrt(t1 * t1 - (b * (-b) - t2 * (a + t1)));
	} else {
		a = a + a;
		y1 = 0;
		SET_HIGH_WORD(y1, hb);
		y2 = b - y1;
		t1 = 0;
		SET_HIGH_WORD(t1, ha + 0x00100000);
		t2 = a - t1;
		w = sqrt(t1 * y1 - (w * (-w) - (t1 * y2 + t2 * b)));
	}
	if (k != 0) {
		uint32_t high;
		t1 = 1.0;
		GET_HIGH_WORD(high, t1);
		SET_HIGH_WORD(t1, high + (k << 20));
		return t1 * w;
	} else
		return w;
}

float
hypotf(float x, float y)
{
	return (float)hypot((double)x, (double)y);
}

long double
hypotl(long double x, long double y)
{
	return (long double)hypot((double)x, (double)y);
}

/* === NaN === */

double
nan(const char *tagp)
{
	(void)tagp;
	return NAN;
}

float
nanf(const char *tagp)
{
	(void)tagp;
	return NAN;
}

long double
nanl(const char *tagp)
{
	(void)tagp;
	return (long double)NAN;
}

/* === nextafter === */

double
nextafter(double x, double y)
{
	int32_t hx, hy, ix, iy;
	uint32_t lx, ly;
	
	EXTRACT_WORDS(hx, lx, x);
	EXTRACT_WORDS(hy, ly, y);
	ix = hx & 0x7fffffff;
	iy = hy & 0x7fffffff;
	
	if (((ix >= 0x7ff00000) && ((ix - 0x7ff00000) | lx) != 0) ||
	    ((iy >= 0x7ff00000) && ((iy - 0x7ff00000) | ly) != 0))
		return x + y;
	
	if (x == y)
		return y;
	
	if ((ix | lx) == 0) {
		INSERT_WORDS(x, hy & 0x80000000, 1);
		return x;
	}
	
	if (hx >= 0) {
		if (hx > hy || ((hx == hy) && (lx > ly))) {
			if (lx == 0)
				hx -= 1;
			lx -= 1;
		} else {
			lx += 1;
			if (lx == 0)
				hx += 1;
		}
	} else {
		if (hy >= 0 || hx > hy || ((hx == hy) && (lx > ly))) {
			if (lx == 0)
				hx -= 1;
			lx -= 1;
		} else {
			lx += 1;
			if (lx == 0)
				hx += 1;
		}
	}
	iy = hx & 0x7ff00000;
	if (iy >= 0x7ff00000)
		return x + x;
	if (iy < 0x00100000) {
		double t = x * x;
		if (t != x) {
			return t;
		}
	}
	INSERT_WORDS(x, hx, lx);
	return x;
}

float
nextafterf(float x, float y)
{
	return (float)nextafter((double)x, (double)y);
}

long double
nextafterl(long double x, long double y)
{
	return (long double)nextafter((double)x, (double)y);
}

double
nexttoward(double x, long double y)
{
	return nextafter(x, (double)y);
}

float
nexttowardf(float x, long double y)
{
	return (float)nextafter((double)x, (double)y);
}

long double
nexttowardl(long double x, long double y)
{
	return nextafterl(x, y);
}

/* === FMA (Fused multiply-add) === */

double
fma(double x, double y, double z)
{
	/* Simplified FMA - not truly fused */
	return (x * y) + z;
}

float
fmaf(float x, float y, float z)
{
	return (float)fma((double)x, (double)y, (double)z);
}

long double
fmal(long double x, long double y, long double z)
{
	return (long double)fma((double)x, (double)y, (double)z);
}

/* === finite (BSD extension) === */

int
finite(double x)
{
	return __isfinite(x);
}