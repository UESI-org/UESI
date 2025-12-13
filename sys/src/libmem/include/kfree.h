#ifndef _KFREE_H_
#define _KFREE_H_

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

void kfree(void *ptr);

/* Free memory and set pointer to NULL (safer) */
#define kfree_safe(ptr)                                                        \
	do {                                                                   \
		kfree(ptr);                                                    \
		(ptr) = NULL;                                                  \
	} while (0)

bool kfree_validate(void *ptr);

#endif