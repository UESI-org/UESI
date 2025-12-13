#include <stdarg.h>
#include <fcntl.h>
#include <syscall.h>
#include <errno.h>

int errno = 0;

/* System call with 0 arguments */
static inline int64_t
syscall0(uint64_t syscall_num)
{
	int64_t ret;
	asm volatile("int $0x80"
	             : "=a"(ret)
	             : "a"(syscall_num)
	             : "memory", "rcx", "r11");
	return ret;
}

/* System call with 1 argument */
static inline int64_t
syscall1(uint64_t syscall_num, uint64_t arg1)
{
	int64_t ret;
	asm volatile("int $0x80"
	             : "=a"(ret)
	             : "a"(syscall_num), "D"(arg1)
	             : "memory", "rcx", "r11");
	return ret;
}

/* System call with 2 arguments */
static inline int64_t
syscall2(uint64_t syscall_num, uint64_t arg1, uint64_t arg2)
{
	int64_t ret;
	asm volatile("int $0x80"
	             : "=a"(ret)
	             : "a"(syscall_num), "D"(arg1), "S"(arg2)
	             : "memory", "rcx", "r11");
	return ret;
}

/* System call with 3 arguments */
static inline int64_t
syscall3(uint64_t syscall_num, uint64_t arg1, uint64_t arg2, uint64_t arg3)
{
	int64_t ret;
	asm volatile("int $0x80"
	             : "=a"(ret)
	             : "a"(syscall_num), "D"(arg1), "S"(arg2), "d"(arg3)
	             : "memory", "rcx", "r11");
	return ret;
}

/* System call with 6 arguments - needed for mmap */
static inline int64_t
syscall6(uint64_t syscall_num,
         uint64_t arg1,
         uint64_t arg2,
         uint64_t arg3,
         uint64_t arg4,
         uint64_t arg5,
         uint64_t arg6)
{
	int64_t ret;
	register uint64_t r10 asm("r10") = arg4;
	register uint64_t r8 asm("r8") = arg5;
	register uint64_t r9 asm("r9") = arg6;

	asm volatile("int $0x80"
	             : "=a"(ret)
	             : "a"(syscall_num),
	               "D"(arg1),
	               "S"(arg2),
	               "d"(arg3),
	               "r"(r10),
	               "r"(r8),
	               "r"(r9)
	             : "memory", "rcx", "r11");
	return ret;
}

static inline int64_t
handle_syscall_result(int64_t ret)
{
	if (ret < 0) {
		errno = (int)(-ret);
		return -1;
	}
	errno = 0;
	return ret;
}

int64_t
open(const char *path, uint32_t flags, mode_t mode)
{
	int64_t ret = syscall3(
	    SYSCALL_OPEN, (uint64_t)path, (uint64_t)flags, (uint64_t)mode);
	return handle_syscall_result(ret);
}

int64_t
close(int fd)
{
	int64_t ret = syscall1(SYSCALL_CLOSE, (uint64_t)fd);
	return handle_syscall_result(ret);
}

int64_t
read(int fd, void *buf, size_t count)
{
	int64_t ret = syscall3(
	    SYSCALL_READ, (uint64_t)fd, (uint64_t)buf, (uint64_t)count);
	return handle_syscall_result(ret);
}

int64_t
write(int fd, const void *buf, size_t count)
{
	int64_t ret = syscall3(
	    SYSCALL_WRITE, (uint64_t)fd, (uint64_t)buf, (uint64_t)count);
	return handle_syscall_result(ret);
}

int64_t
creat(const char *path, mode_t mode)
{
	int64_t ret = syscall2(SYSCALL_CREAT, (uint64_t)path, (uint64_t)mode);
	return handle_syscall_result(ret);
}

int64_t
openat(int dirfd, const char *pathname, uint32_t flags, mode_t mode)
{
	/* System call with 4 arguments */
	int64_t ret;
	asm volatile("int $0x80"
	             : "=a"(ret)
	             : "a"((uint64_t)SYSCALL_OPENAT),
	               "D"((uint64_t)dirfd),
	               "S"((uint64_t)pathname),
	               "d"((uint64_t)flags),
	               "r"((uint64_t)mode) /* This will go in r10 */
	             : "memory", "rcx", "r11");
	return handle_syscall_result(ret);
}

int
mkdir(const char *path, mode_t mode)
{
	int64_t ret = syscall2(SYSCALL_MKDIR, (uint64_t)path, (uint64_t)mode);
	return (int)handle_syscall_result(ret);
}

int
fcntl(int fd, int cmd, ...)
{
	va_list args;
	va_start(args, cmd);

	/* fcntl can take 0, 1, or more arguments depending on cmd */
	uint64_t arg = 0;

	/* Commands that take an argument */
	switch (cmd) {
	case F_DUPFD:
	case F_DUPFD_CLOEXEC:
	case F_SETFD:
	case F_SETFL:
	case F_SETOWN:
	case F_SETLK:
	case F_SETLKW:
	case F_GETLK:
		arg = va_arg(args, uint64_t);
		break;
	default:
		arg = 0;
		break;
	}

	va_end(args);

	int64_t ret = syscall3(SYSCALL_FCNTL, (uint64_t)fd, (uint64_t)cmd, arg);
	return (int)handle_syscall_result(ret);
}

int
dup(int oldfd)
{
	int64_t ret = syscall1(SYSCALL_DUP, (uint64_t)oldfd);
	return (int)handle_syscall_result(ret);
}

int
dup2(int oldfd, int newfd)
{
	int64_t ret = syscall2(SYSCALL_DUP2, (uint64_t)oldfd, (uint64_t)newfd);
	return (int)handle_syscall_result(ret);
}

int
stat(const char *path, struct stat *buf)
{
	int64_t ret = syscall2(SYSCALL_STAT, (uint64_t)path, (uint64_t)buf);
	return (int)handle_syscall_result(ret);
}

int
fstat(int fd, struct stat *buf)
{
	int64_t ret = syscall2(SYSCALL_FSTAT, (uint64_t)fd, (uint64_t)buf);
	return (int)handle_syscall_result(ret);
}

int
lstat(const char *path, struct stat *buf)
{
	int64_t ret = syscall2(SYSCALL_LSTAT, (uint64_t)path, (uint64_t)buf);
	return (int)handle_syscall_result(ret);
}

off_t
lseek(int fd, off_t offset, int whence)
{
	int64_t ret = syscall3(
	    SYSCALL_LSEEK, (uint64_t)fd, (uint64_t)offset, (uint64_t)whence);

	if (ret < 0) {
		errno = (int)(-ret);
		return (off_t)-1;
	}

	errno = 0;
	return (off_t)ret;
}

pid_t
fork(void)
{
	int64_t ret = syscall0(SYSCALL_FORK);

	if (ret < 0) {
		errno = (int)(-ret);
		return -1;
	}

	return (pid_t)ret;
}

void *
mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
{
	int64_t ret = syscall6(SYSCALL_MMAP,
	                       (uint64_t)addr,
	                       (uint64_t)length,
	                       (uint64_t)prot,
	                       (uint64_t)flags,
	                       (uint64_t)fd,
	                       (uint64_t)offset);

	if (ret < 0 && ret >= -4096) {
		errno = (int)(-ret);
		return MAP_FAILED;
	}

	errno = 0;
	return (void *)ret;
}

int
munmap(void *addr, size_t length)
{
	int64_t ret =
	    syscall2(SYSCALL_MUNMAP, (uint64_t)addr, (uint64_t)length);
	return (int)handle_syscall_result(ret);
}

void *
brk(void *addr)
{
	int64_t ret = syscall1(SYSCALL_BRK, (uint64_t)addr);

	if (ret < 0) {
		errno = (int)(-ret);
		return (void *)-1;
	}

	return (void *)ret;
}

void *
sbrk(intptr_t increment)
{
	void *current = brk(NULL);
	if (current == (void *)-1) {
		return (void *)-1;
	}

	if (increment == 0) {
		return current;
	}

	void *new_brk = (void *)((uint64_t)current + increment);

	if (brk(new_brk) == (void *)-1) {
		return (void *)-1;
	}

	return current;
}

pid_t
getpid(void)
{
	return (pid_t)syscall0(SYSCALL_GETPID);
}

int
mprotect(void *addr, size_t len, int prot)
{
	int64_t ret = syscall3(
	    SYSCALL_MPROTECT, (uint64_t)addr, (uint64_t)len, (uint64_t)prot);
	return (int)handle_syscall_result(ret);
}

void
exit(int status)
{
	syscall1(SYSCALL_EXIT, (uint64_t)status);
	__builtin_unreachable();
}

int
sysinfo(struct sysinfo *info)
{
	int64_t ret = syscall1(SYSCALL_SYSINFO, (uint64_t)info);
	return (int)handle_syscall_result(ret);
}

int
uname(struct utsname *buf)
{
	int64_t ret = syscall1(SYSCALL_UNAME, (uint64_t)buf);
	return (int)handle_syscall_result(ret);
}

int
gethostname(char *name, size_t len)
{
	int64_t ret =
	    syscall2(SYSCALL_GETHOSTNAME, (uint64_t)name, (uint64_t)len);
	return (int)handle_syscall_result(ret);
}

pid_t
getppid(void)
{
	return (pid_t)syscall0(SYSCALL_GETPPID);
}

int
gethostid(void)
{
	/* gethostid returns a host ID value, not an error code */
	return (int)syscall0(SYSCALL_GETHOSTID);
}