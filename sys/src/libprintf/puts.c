#include <puts.h>
#include <stddef.h>

extern void _putchar(char character);

int
puts(const char *str)
{
	if (str == NULL) {
		return -1;
	}

	int count = 0;

	while (*str) {
		_putchar(*str++);
		count++;
	}

	_putchar('\n');
	count++;

	return count;
}

int
putstr(const char *str)
{
	if (str == NULL) {
		return -1;
	}

	int count = 0;

	while (*str) {
		_putchar(*str++);
		count++;
	}

	return count;
}

int
putchar(int c)
{
	_putchar((char)c);
	return c;
}

int
putchar_n(int c, size_t n)
{
	if (n == 0) {
		return 0;
	}

	for (size_t i = 0; i < n; i++) {
		_putchar((char)c);
	}

	return (int)n;
}