#include <sys/types.h>
#include "mieeefp.h"

fp_except _softfloat_float_exception_flags = 0;
fp_except _softfloat_float_exception_mask = 0;

fp_rnd _softfloat_float_rounding_mode = FP_RN;

void
float_raise(fp_except flags)
{
	_softfloat_float_exception_flags |= flags;
}

void
float_set_inexact(void)
{
	float_raise(FP_X_IMP);
}

fp_rnd
fpgetround(void)
{
	return _softfloat_float_rounding_mode;
}

fp_rnd
fpsetround(fp_rnd new_mode)
{
	fp_rnd old_mode = _softfloat_float_rounding_mode;
	_softfloat_float_rounding_mode = new_mode;
	return old_mode;
}

fp_except
fpgetmask(void)
{
	return _softfloat_float_exception_mask;
}

fp_except
fpsetmask(fp_except new_mask)
{
	fp_except old_mask = _softfloat_float_exception_mask;
	_softfloat_float_exception_mask = new_mask;
	return old_mask;
}

fp_except
fpgetsticky(void)
{
	return _softfloat_float_exception_flags;
}

fp_except
fpsetsticky(fp_except new_flags)
{
	fp_except old_flags = _softfloat_float_exception_flags;
	_softfloat_float_exception_flags = new_flags;
	return old_flags;
}