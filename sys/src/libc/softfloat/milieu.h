/*      $OpenBSD: milieu.h,v 1.3 2016/09/12 19:41:20 guenther Exp $     */

/*
===============================================================================
This C header file is part of the SoftFloat IEC/IEEE Floating-point
Arithmetic Package, Release 2a.

Written by John R. Hauser.  This work was made possible in part by the
International Computer Science Institute, located at Suite 600, 1947 Center
Street, Berkeley, California 94704.  Funding was partially provided by the
National Science Foundation under grant MIP-9311980.  The original version
of this code was written as part of a project to build a fixed-point vector
processor in collaboration with the University of California at Berkeley,
overseen by Profs. Nelson Morgan and John Wawrzynek.  More information
is available through the Web page `http://HTTP.CS.Berkeley.EDU/~jhauser/
arithmetic/SoftFloat.html'.

THIS SOFTWARE IS DISTRIBUTED AS IS, FOR FREE.  Although reasonable effort
has been made to avoid it, THIS SOFTWARE MAY CONTAIN FAULTS THAT WILL AT
TIMES RESULT IN INCORRECT BEHAVIOR.  USE OF THIS SOFTWARE IS RESTRICTED TO
PERSONS AND ORGANIZATIONS WHO CAN AND WILL TAKE FULL RESPONSIBILITY FOR ANY
AND ALL LOSSES, COSTS, OR OTHER PROBLEMS ARISING FROM ITS USE.

Derivative works are acceptable, even for commercial purposes, so long as
(1) they include prominent notice that the work is derivative, and (2) they
include prominent notice akin to these four paragraphs for those parts of
this code that are retained.
===============================================================================
*/

#ifndef _MILIEU_H_
#define _MILIEU_H_

/*
-------------------------------------------------------------------------------
Include common integer types and constants.  `INLINE' must be defined as a
macro that can be used as a prefix on a function definition to make it an
inlined function.  This file also provides function declarations for an inline
barrier function and for shift operations.
-------------------------------------------------------------------------------
*/

/* Basic integer types for softfloat */
typedef signed char int8;
typedef unsigned char uint8;
typedef signed short int int16;
typedef unsigned short int uint16;
typedef signed int int32;
typedef unsigned int uint32;
typedef signed long long int int64;
typedef unsigned long long int uint64;

/* Legacy type names used in softfloat-macros.h */
typedef uint16 bits16;
typedef uint32 bits32;
typedef uint64 bits64;

/* Signed versions */
typedef int32 sbits32;
typedef int64 sbits64;

/* Boolean/flag type used throughout softfloat
 * NOTE: Changed from int8 to int to match GCC comparison functions
 * which return int, not signed char
 */
typedef int flag;

/* Constants */
#define LIT64(a) a##LL

/* For kernel/freestanding environment, __dso_protected is not supported */
#ifndef __dso_protected
#define __dso_protected
#endif

/* Inline directive */
#define INLINE static inline

/*
 * Signal handling stubs for kernel environment.
 * In a freestanding kernel, we don't have signal.h or raise().
 * Define SIGFPE and a stub raise() function that does nothing.
 */
#ifdef _KERNEL
#define SIGFPE 8 /* Floating point exception signal number */
static inline int
raise(int sig __attribute__((unused)))
{
	/* In kernel mode, we don't raise signals */
	return 0;
}
#endif

/*
 * Macros for manipulating floating-point values.
 * FLOAT32_MANGLE and FLOAT64_MANGLE are used to manipulate the bit
 * representation of floats (e.g., to flip the sign bit).
 * On most architectures, these are identity macros.
 */
#ifndef FLOAT32_MANGLE
#define FLOAT32_MANGLE(x) (x)
#endif

#ifndef FLOAT64_MANGLE
#define FLOAT64_MANGLE(x) (x)
#endif

#endif /* _MILIEU_H_ */