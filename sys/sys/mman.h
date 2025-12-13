#ifndef _SYS_MMAN_H_
#define _SYS_MMAN_H_

#include <sys/types.h>

#define PROT_NONE 0x00  /* No access */
#define PROT_READ 0x01  /* Pages can be read */
#define PROT_WRITE 0x02 /* Pages can be written */
#define PROT_EXEC 0x04  /* Pages can be executed */

#define MAP_SHARED 0x0001    /* Share changes */
#define MAP_PRIVATE 0x0002   /* Changes are private */
#define MAP_FIXED 0x0010     /* Interpret addr exactly */
#define MAP_ANONYMOUS 0x0020 /* Don't use a file */
#define MAP_ANON MAP_ANONYMOUS

#define MAP_FAILED ((void *)-1)

#ifndef _KERNEL
void *mmap(
    void *addr, size_t length, int prot, int flags, int fd, off_t offset);
int munmap(void *addr, size_t length);
int mprotect(void *addr, size_t len, int prot);
#endif

#endif