#ifndef _CTYPE_H_
#define _CTYPE_H_

#define	_U	0x01
#define	_L	0x02
#define	_N	0x04		/* digit */
#define	_S	0x08		/* whitespace */
#define	_P	0x10
#define	_C	0x20
#define	_X	0x40		/* hexadecimal digit */
#define	_B	0x80

/*
 * Character classification table
 * External definition - must be provided by ctype_.c
 */
extern const unsigned char _ctype_[256];

/*
 * Character classification macros
 * Note: The +1 offset handles EOF (-1) safely
 */
#define	isdigit(c)	((_ctype_ + 1)[(unsigned char)(c)] & _N)
#define	islower(c)	((_ctype_ + 1)[(unsigned char)(c)] & _L)
#define	isspace(c)	((_ctype_ + 1)[(unsigned char)(c)] & _S)
#define	ispunct(c)	((_ctype_ + 1)[(unsigned char)(c)] & _P)
#define	isupper(c)	((_ctype_ + 1)[(unsigned char)(c)] & _U)
#define	isalpha(c)	((_ctype_ + 1)[(unsigned char)(c)] & (_U|_L))
#define	isxdigit(c)	((_ctype_ + 1)[(unsigned char)(c)] & (_N|_X))
#define	isalnum(c)	((_ctype_ + 1)[(unsigned char)(c)] & (_U|_L|_N))
#define	isprint(c)	((_ctype_ + 1)[(unsigned char)(c)] & (_P|_U|_L|_N|_B))
#define	isgraph(c)	((_ctype_ + 1)[(unsigned char)(c)] & (_P|_U|_L|_N))
#define	iscntrl(c)	((_ctype_ + 1)[(unsigned char)(c)] & _C)
#define	isblank(c)	((_ctype_ + 1)[(unsigned char)(c)] & _B)

#define	isascii(c)	((unsigned)(c) <= 0177)
#define	toascii(c)	((c) & 0177)

/*
 * Case conversion macros
 * These perform proper boundary checking
 */
#define	_toupper(c)	((c) - 'a' + 'A')
#define	_tolower(c)	((c) - 'A' + 'a')

static inline int toupper(int c)
{
	if (islower(c))
		return _toupper(c);
	return c;
}

static inline int tolower(int c)
{
	if (isupper(c))
		return _tolower(c);
	return c;
}

#ifndef EOF
#define EOF	(-1)
#endif

#endif /* _CTYPE_H_ */