#ifndef	_SYS_CDEFS_H_
#define	_SYS_CDEFS_H_

/*
 * Macro to test if we're using a GNU C compiler of a specific vintage
 * or later, for e.g. features that appeared in a particular version
 * of GNU C.  Usage:
 *
 *	#if __GNUC_PREREQ__(major, minor)
 *	...cool feature...
 *	#else
 *	...delete feature...
 *	#endif
 */
#ifdef __GNUC__
#define	__GNUC_PREREQ__(x, y)						\
	((__GNUC__ == (x) && __GNUC_MINOR__ >= (y)) ||			\
	 (__GNUC__ > (x)))
#else
#define	__GNUC_PREREQ__(x, y)	0
#endif

/*
 * Compiler-dependent macros to declare that functions take printf-like
 * or scanf-like arguments.  They are null except for versions of gcc
 * that are known to support the features properly.
 */
#if __GNUC_PREREQ__(2, 7)
#define	__printflike(fmtarg, firstvararg)	\
	__attribute__((__format__ (__printf__, fmtarg, firstvararg)))
#define	__scanflike(fmtarg, firstvararg)	\
	__attribute__((__format__ (__scanf__, fmtarg, firstvararg)))
#else
#define	__printflike(fmtarg, firstvararg)
#define	__scanflike(fmtarg, firstvararg)
#endif

/*
 * GNU C version 2.96 added explicit branch prediction so that
 * the CPU back-end can hint the processor and also so that
 * code blocks can be reordered such that the predicted path
 * sees a more linear flow, thus improving cache behavior, etc.
 *
 * The following two macros provide us with a way to use this
 * compiler feature.  Use __predict_true() if you expect the expression
 * to evaluate to true, and __predict_false() if you expect the
 * expression to evaluate to false.
 *
 * A few notes about usage:
 *
 *	* Generally, __predict_false() error condition checks (unless
 *	  you have some _strong_ reason to do otherwise, in which case
 *	  document it), and/or __predict_true() `no-error' condition
 *	  checks, assuming you want to optimize for the no-error case.
 *
 *	* Other than that, if you don't know the likelihood of a test
 *	  succeeding from empirical or other `hard' evidence, don't
 *	  make predictions.
 *
 *	* These are meant to be used in places that are run `a lot'.
 *	  It is wasteful to make predictions in code that is run
 *	  seldomly (e.g. at subsystem initialization time) as the
 *	  basic block reordering that this affects can often generate
 *	  larger code.
 */
#if __GNUC_PREREQ__(2, 96)
#define	__predict_true(exp)	__builtin_expect((exp) != 0, 1)
#define	__predict_false(exp)	__builtin_expect((exp) != 0, 0)
#else
#define	__predict_true(exp)	(exp)
#define	__predict_false(exp)	(exp)
#endif

/*
 * Compiler-dependent macros to help declare dead (non-returning) and
 * pure (no side effects) functions, and unused variables.
 */
#if __GNUC_PREREQ__(2, 5)
#define	__dead		__attribute__((__noreturn__))
#else
#define	__dead
#endif

#if __GNUC_PREREQ__(2, 96)
#define	__pure		__attribute__((__pure__))
#else
#define	__pure
#endif

#if __GNUC_PREREQ__(2, 7)
#define	__unused	__attribute__((__unused__))
#else
#define	__unused
#endif

#if __GNUC_PREREQ__(3, 1)
#define	__used		__attribute__((__used__))
#else
#define	__used		__unused
#endif

#if __GNUC_PREREQ__(2, 7)
#define	__packed	__attribute__((__packed__))
#define	__aligned(x)	__attribute__((__aligned__(x)))
#define	__section(x)	__attribute__((__section__(x)))
#else
#define	__packed
#define	__aligned(x)
#define	__section(x)
#endif

#if __GNUC_PREREQ__(4, 3) || defined(__clang__)
#define	__alloc_size(x)	__attribute__((__alloc_size__(x)))
#define	__alloc_align(x) __attribute__((__alloc_align__(x)))
#else
#define	__alloc_size(x)
#define	__alloc_align(x)
#endif

#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 201112L
#if __GNUC_PREREQ__(2, 7)
#define	_Static_assert(x, y)	__extension__ _Static_assert(x, y)
#endif
#endif

/*
 * C99 Static array indices in function parameter declarations
 */
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)
#define	__min_size(x)	static (x)
#else
#define	__min_size(x)	(x)
#endif

/*
 * The __CONCAT macro is used to concatenate parts of symbol names, e.g.
 * with "#define OLD(foo) __CONCAT(old,foo)", OLD(foo) produces oldfoo.
 * The __CONCAT macro is a bit tricky -- make sure you don't put spaces
 * in between its arguments.  __CONCAT can also concatenate double-quoted
 * strings produced by the __STRING macro, but this only works with ANSI C.
 */
#if defined(__STDC__) || defined(__cplusplus)
#define	__P(protos)	protos		/* full-blown ANSI C */
#define	__CONCAT(x,y)	x ## y
#define	__STRING(x)	#x

#define	__const		const		/* define reserved names to standard */
#define	__signed	signed
#define	__volatile	volatile
#if defined(__cplusplus)
#define	__inline	inline		/* convert to C++ keyword */
#else
#if !defined(__GNUC__) && !defined(__lint__)
#define	__inline			/* delete GCC keyword */
#endif /* !__GNUC__  && !__lint__ */
#endif /* !__cplusplus */

#else	/* !(__STDC__ || __cplusplus) */
#define	__P(protos)	()		/* traditional C preprocessor */
#define	__CONCAT(x,y)	x/**/y
#define	__STRING(x)	"x"

#ifndef __GNUC__
#define	__const				/* delete pseudo-ANSI C keywords */
#define	__inline
#define	__signed
#define	__volatile
#endif	/* !__GNUC__ */

/*
 * In non-ANSI C environments, new programs will want ANSI-only C keywords
 * deleted from the program and old programs will want them left alone.
 * Programs using the ANSI C keywords const, inline etc. as normal
 * identifiers should define -DNO_ANSI_KEYWORDS.
 * Programs that want to use the ANSI keywords as keywords should 
 * define -DANSI_KEYWORDS.
 */
#ifndef	NO_ANSI_KEYWORDS
#define	const		__const		/* convert ANSI C keywords */
#define	inline		__inline
#define	signed		__signed
#define	volatile	__volatile
#endif /* !NO_ANSI_KEYWORDS */
#endif	/* !(__STDC__ || __cplusplus) */

/*
 * Used for internal auditing of the kernel sources.
 */
#ifdef __KERNEL_RCSID
#define __KERNEL_RCSID(n, s)	/* nothing */
#endif

/*
 * The following macro is used to remove const cast-away warnings
 * from gcc -Wcast-qual; it should be used with caution because it
 * can hide valid errors; in particular most valid uses are in
 * situations where the API requires it, not to cast away string
 * constants. We don't use *intptr_t on purpose here and we are
 * explicit about unsigned long so that we don't have additional
 * dependencies.
 */
#define	__UNCONST(a)	((void *)(unsigned long)(const void *)(a))

/*
 * The following macro is used to remove the volatile cast-away warnings
 * from gcc -Wcast-qual; as above it should be used with caution
 * because it can hide valid errors or warnings.  Valid uses include
 * making it possible to pass a volatile pointer to memset().
 * For the same reasons as above, we use unsigned long and not intptr_t.
 */
#define	__UNVOLATILE(a)	((void *)(unsigned long)(volatile void *)(a))

/*
 * GCC2 provides __extension__ to suppress warnings for various GNU C
 * language extensions under "-ansi -pedantic".
 */
#if !__GNUC_PREREQ__(2, 0)
#define	__extension__		/* delete __extension__ if non-gcc or gcc1 */
#endif

/*
 * GCC1 and some versions of GCC2 declare dead (non-returning) and
 * pure (no side effects) functions using "volatile" and "const";
 * unfortunately, these then cause warnings under "-ansi -pedantic".
 * GCC2 uses a new, peculiar __attribute__((attrs)) style.  All of
 * these work for GNU C++ (modulo a slight glitch in the C++ grammar
 * in the distribution version of 2.5.5).
 */
#if !__GNUC_PREREQ__(2, 5)
#define	__attribute__(x)	/* delete __attribute__ if non-gcc or gcc1 */
#endif

#if !(defined(__clang__) && __has_feature(nullability))
#ifndef _Nonnull
#define	_Nonnull
#endif
#ifndef _Nullable
#define	_Nullable
#endif
#ifndef _Null_unspecified
#define	_Null_unspecified
#endif
#endif

/*
 * C++11 adds the noreturn attribute, but as [[noreturn]] not __attribute__.
 */
#if defined(__cplusplus) && __cplusplus >= 201103L
#define	__noreturn	[[noreturn]]
#else
#define	__noreturn	__dead
#endif

/*
 * Macro to test if we are compiling for the kernel.
 */
#if defined(_KERNEL) || defined(_STANDALONE)
#define	__KERNEL_PROTECT	/* nothing */
#else
#define	__KERNEL_PROTECT	__attribute__((__deprecated__))
#endif

/*
 * We use __weak_symbol as a flag to the compiler to generate weak symbols.
 * This is useful for library functions that should be overridable.
 */
#if __GNUC_PREREQ__(2, 7)
#define	__weak_symbol	__attribute__((__weak__))
#else
#define	__weak_symbol
#endif

/*
 * We use __strong_alias and __weak_alias to create aliases between symbols.
 */
#define	__strong_alias(alias, sym)	\
	__asm__(".global " #alias " ; " #alias " = " #sym)
#define	__weak_alias(alias, sym)	\
	__asm__(".weak " #alias " ; " #alias " = " #sym)

/*
 * __returns_twice makes the compiler not assume the function
 * only returns once.  This affects registerisation of variables:
 * even local variables need to be in memory across such a call.
 * Example: setjmp()
 */
#if __GNUC_PREREQ__(4, 1)
#define	__returns_twice	__attribute__((__returns_twice__))
#else
#define	__returns_twice
#endif

/*
 * __bounded__ is a OpenBSD extension to GCC to allow the compiler
 * to perform bounds checking on arrays and strings
 */
#ifdef __OpenBSD_GCC__
#define	__bounded__(x, y, z)	__attribute__((__bounded__(x,y,z)))
#else
#define	__bounded__(x, y, z)
#endif

/*
 * Hide kernel and other potentially dangerous symbols
 */
#define	__BEGIN_HIDDEN_DECLS	_Pragma("GCC visibility push(hidden)")
#define	__END_HIDDEN_DECLS	_Pragma("GCC visibility pop")

/*
 * C++ needs to know that types and declarations are C, not C++.
 */
#ifdef __cplusplus
#define	__BEGIN_DECLS	extern "C" {
#define	__END_DECLS	}
#else
#define	__BEGIN_DECLS
#define	__END_DECLS
#endif

/*
 * GNU C version 2.96 added explicit branch prediction so that
 * the CPU back-end can hint the processor and also so that
 * code blocks can be reordered such that the predicted path
 * sees a more linear flow, thus improving cache behavior, etc.
 */
#if __GNUC_PREREQ__(3, 0)
#define	__likely(x)	__builtin_expect(!!(x), 1)
#define	__unlikely(x)	__builtin_expect(!!(x), 0)
#else
#define	__likely(x)	(x)
#define	__unlikely(x)	(x)
#endif

/*
 * For symbol versioning we need to rename symbols.
 */
#define	__sym_compat(sym, impl, verid)
#define	__sym_default(sym, impl, verid)

/*
 * Compiler barrier - prevent reordering of reads and writes.
 */
#define	__compiler_membar()	__asm __volatile("" ::: "memory")

#endif /* !_SYS_CDEFS_H_ */