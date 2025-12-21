#include <errno.h>

#undef errno

static int __errno_storage = 0;

int *
__errno(void)
{
    return &__errno_storage;
}