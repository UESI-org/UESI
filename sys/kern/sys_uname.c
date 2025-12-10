#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/param.h>
#include <string.h>

extern const char ostype[];
extern const char osrelease[];
extern const char version[];
extern int gethostname(char *name, size_t len);

int kern_uname(struct utsname *name) {
    if (name == NULL) {
        return -1;
    }

    memset(name, 0, sizeof(struct utsname));

    strncpy(name->sysname, ostype, SYS_NMLN - 1);
    name->sysname[SYS_NMLN - 1] = '\0';

    if (gethostname(name->nodename, SYS_NMLN) < 0) {
        strncpy(name->nodename, "localhost", SYS_NMLN - 1);
        name->nodename[SYS_NMLN - 1] = '\0';
    }

    strncpy(name->release, osrelease, SYS_NMLN - 1);
    name->release[SYS_NMLN - 1] = '\0';

    strncpy(name->version, version, SYS_NMLN - 1);
    name->version[SYS_NMLN - 1] = '\0';

    strncpy(name->machine, "x86_64", SYS_NMLN - 1);
    name->machine[SYS_NMLN - 1] = '\0';

    return 0;
}