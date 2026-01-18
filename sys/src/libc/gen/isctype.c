#define _ANSI_LIBRARY
#include <ctype.h>
#include <stdio.h>

#undef isalnum
int
isalnum(int c)
{
	return (c == EOF ? 0 : ((_ctype_ + 1)[(unsigned char)c] &
	    (_CTYPE_U|_CTYPE_L|_CTYPE_N)));
}

#undef isalpha
int
isalpha(int c)
{
	return (c == EOF ? 0 : ((_ctype_ + 1)[(unsigned char)c] &
	    (_CTYPE_U|_CTYPE_L)));
}

#undef isblank
int
isblank(int c)
{
	return (c == ' ' || c == '\t');
}

#undef iscntrl
int
iscntrl(int c)
{
	return (c == EOF ? 0 : ((_ctype_ + 1)[(unsigned char)c] & _CTYPE_C));
}

#undef isdigit
int
isdigit(int c)
{
	return (c == EOF ? 0 : ((_ctype_ + 1)[(unsigned char)c] & _CTYPE_N));
}

#undef isgraph
int
isgraph(int c)
{
	return (c == EOF ? 0 : ((_ctype_ + 1)[(unsigned char)c] &
	    (_CTYPE_P|_CTYPE_U|_CTYPE_L|_CTYPE_N)));
}

#undef islower
int
islower(int c)
{
	return (c == EOF ? 0 : ((_ctype_ + 1)[(unsigned char)c] & _CTYPE_L));
}

#undef isprint
int
isprint(int c)
{
	return (c == EOF ? 0 : ((_ctype_ + 1)[(unsigned char)c] &
	    (_CTYPE_P|_CTYPE_U|_CTYPE_L|_CTYPE_N|_CTYPE_B)));
}

#undef ispunct
int
ispunct(int c)
{
	return (c == EOF ? 0 : ((_ctype_ + 1)[(unsigned char)c] & _CTYPE_P));
}

#undef isspace
int
isspace(int c)
{
	return (c == EOF ? 0 : ((_ctype_ + 1)[(unsigned char)c] & _CTYPE_S));
}

#undef isupper
int
isupper(int c)
{
	return (c == EOF ? 0 : ((_ctype_ + 1)[(unsigned char)c] & _CTYPE_U));
}

#undef isxdigit
int
isxdigit(int c)
{
	return (c == EOF ? 0 : ((_ctype_ + 1)[(unsigned char)c] &
	    (_CTYPE_N|_CTYPE_X)));
}

#undef isascii
int
isascii(int c)
{
	return ((unsigned int)c <= 0177);
}

#undef toascii
int
toascii(int c)
{
	return (c & 0177);
}

#undef _toupper
int
_toupper(int c)
{
	return (c - 'a' + 'A');
}

#undef _tolower
int
_tolower(int c)
{
	return (c - 'A' + 'a');
}