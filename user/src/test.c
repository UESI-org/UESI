#include <syscall.h>

int
main(void)
{
	write(1, "Hello, Userland!\n", 17);
	return 0;
}