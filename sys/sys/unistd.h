#ifndef _SYS_UNISTD_H_
#define _SYS_UNISTD_H_

#include <sys/cdefs.h>

#define _POSIX_ADVISORY_INFO (-1)
#define _POSIX_ASYNCHRONOUS_IO (-1)
#define _POSIX_BARRIERS 200809L
#define _POSIX_CHOWN_RESTRICTED 1
#define _POSIX_CLOCK_SELECTION (-1)
#define _POSIX_CPUTIME (-1)
#define _POSIX_FSYNC (-1)
#define _POSIX_IPV6 0
#define _POSIX_JOB_CONTROL 1
#define _POSIX_MAPPED_FILES (-1)
#define _POSIX_MEMLOCK (-1)
#define _POSIX_MEMLOCK_RANGE (-1)
#define _POSIX_MEMORY_PROTECTION (-1)
#define _POSIX_MESSAGE_PASSING (-1)
#define _POSIX_MONOTONIC_CLOCK 200809L
#define _POSIX_NO_TRUNC 1
#define _POSIX_PRIORITIZED_IO (-1)
#define _POSIX_PRIORITY_SCHEDULING (-1)
#define _POSIX_RAW_SOCKETS (-1)
#define _POSIX_READER_WRITER_LOCKS 200809L
#define _POSIX_REALTIME_SIGNALS (-1)
#define _POSIX_REGEXP 1
#define _POSIX_SAVED_IDS 1
#define _POSIX_SEMAPHORES (-1)
#define _POSIX_SHARED_MEMORY_OBJECTS (-1)
#define _POSIX_SHELL 1
#define _POSIX_SPAWN (-1)
#define _POSIX_SPIN_LOCKS 200809L
#define _POSIX_SPORADIC_SERVER (-1)
#define _POSIX_SYNCHRONIZED_IO (-1)
#define _POSIX_THREAD_ATTR_STACKADDR (-1)
#define _POSIX_THREAD_ATTR_STACKSIZE (-1)
#define _POSIX_THREAD_CPUTIME (-1)
#define _POSIX_THREAD_PRIO_INHERIT (-1)
#define _POSIX_THREAD_PRIO_PROTECT (-1)
#define _POSIX_THREAD_PRIORITY_SCHEDULING (-1)
#define _POSIX_THREAD_PROCESS_SHARED (-1)
#define _POSIX_THREAD_ROBUST_PRIO_INHERIT (-1)
#define _POSIX_THREAD_ROBUST_PRIO_PROTECT (-1)
#define _POSIX_THREAD_SAFE_FUNCTIONS (-1)
#define _POSIX_THREAD_SPORADIC_SERVER (-1)
#define _POSIX_THREADS (-1)
#define _POSIX_TIMEOUTS (-1)
#define _POSIX_TIMERS (-1)
#define _POSIX_TYPED_MEMORY_OBJECTS (-1)
#define _POSIX_VDISABLE ((unsigned char)'\377')
#define _POSIX2_C_BIND 200809L
#define _POSIX2_C_DEV (-1)
#define _POSIX2_CHAR_TERM 1
#define _POSIX2_FORT_DEV (-1)
#define _POSIX2_FORT_RUN (-1)
#define _POSIX2_LOCALEDEF (-1)
#define _POSIX2_PBS (-1)
#define _POSIX2_PBS_ACCOUNTING (-1)
#define _POSIX2_PBS_CHECKPOINT (-1)
#define _POSIX2_PBS_LOCATE (-1)
#define _POSIX2_PBS_MESSAGE (-1)
#define _POSIX2_PBS_TRACK (-1)
#define _POSIX2_SW_DEV (-1)
#define _POSIX2_UPE 200809L
#define _XOPEN_CRYPT (-1)
#define _XOPEN_ENH_I18N (-1)
#define _XOPEN_LEGACY (-1)
#define _XOPEN_REALTIME (-1)
#define _XOPEN_REALTIME_THREADS (-1)
#define _XOPEN_SHM (-1)
#define _XOPEN_STREAMS (-1)
#define _XOPEN_UNIX (-1)

#define _POSIX_TIMESTAMP_RESOLUTION 1000000000

/* access function */
#define F_OK 0   /* test for existence of file */
#define X_OK 0x01 /* test for execute or search permission */
#define W_OK 0x02 /* test for write permission */
#define R_OK 0x04 /* test for read permission */

/* whence values for lseek(2) */
#define SEEK_SET 0 /* set file offset to offset */
#define SEEK_CUR 1 /* set file offset to current plus offset */
#define SEEK_END 2 /* set file offset to EOF plus offset */

#if __BSD_VISIBLE
/* whence values for lseek(2); renamed by POSIX 1003.1 */
#define L_SET SEEK_SET
#define L_INCR SEEK_CUR
#define L_XTND SEEK_END
#endif

#define _PC_LINK_MAX 1
#define _PC_MAX_CANON 2
#define _PC_MAX_INPUT 3
#define _PC_NAME_MAX 4
#define _PC_PATH_MAX 5
#define _PC_PIPE_BUF 6
#define _PC_CHOWN_RESTRICTED 7
#define _PC_NO_TRUNC 8
#define _PC_VDISABLE 9
#define _PC_2_SYMLINKS 10
#define _PC_ALLOC_SIZE_MIN 11
#define _PC_ASYNC_IO 12
#define _PC_FILESIZEBITS 13
#define _PC_PRIO_IO 14
#define _PC_REC_INCR_XFER_SIZE 15
#define _PC_REC_MAX_XFER_SIZE 16
#define _PC_REC_MIN_XFER_SIZE 17
#define _PC_REC_XFER_ALIGN 18
#define _PC_SYMLINK_MAX 19
#define _PC_SYNC_IO 20
#define _PC_TIMESTAMP_RESOLUTION 21

#define _CS_PATH 1

#define _SC_ARG_MAX 1
#define _SC_CHILD_MAX 2
#define _SC_CLK_TCK 3
#define _SC_NGROUPS_MAX 4
#define _SC_OPEN_MAX 5
#define _SC_JOB_CONTROL 6
#define _SC_SAVED_IDS 7
#define _SC_VERSION 8
#define _SC_BC_BASE_MAX 9
#define _SC_BC_DIM_MAX 10
#define _SC_BC_SCALE_MAX 11
#define _SC_BC_STRING_MAX 12
#define _SC_COLL_WEIGHTS_MAX 13
#define _SC_EXPR_NEST_MAX 14
#define _SC_LINE_MAX 15
#define _SC_RE_DUP_MAX 16
#define _SC_2_VERSION 17
#define _SC_2_C_BIND 18
#define _SC_2_C_DEV 19
#define _SC_2_CHAR_TERM 20
#define _SC_2_FORT_DEV 21
#define _SC_2_FORT_RUN 22
#define _SC_2_LOCALEDEF 23
#define _SC_2_SW_DEV 24
#define _SC_2_UPE 25
#define _SC_STREAM_MAX 26
#define _SC_TZNAME_MAX 27
#define _SC_PAGESIZE 28
#define _SC_PAGE_SIZE _SC_PAGESIZE
#define _SC_FSYNC 29
#define _SC_XOPEN_SHM 30
#define _SC_SEM_NSEMS_MAX 31
#define _SC_SEM_VALUE_MAX 32
#define _SC_HOST_NAME_MAX 33
#define _SC_MONOTONIC_CLOCK 34
#define _SC_2_PBS 35
#define _SC_2_PBS_ACCOUNTING 36
#define _SC_2_PBS_CHECKPOINT 37
#define _SC_2_PBS_LOCATE 38
#define _SC_2_PBS_MESSAGE 39
#define _SC_2_PBS_TRACK 40
#define _SC_ADVISORY_INFO 41
#define _SC_AIO_LISTIO_MAX 42
#define _SC_AIO_MAX 43
#define _SC_AIO_PRIO_DELTA_MAX 44
#define _SC_ASYNCHRONOUS_IO 45
#define _SC_ATEXIT_MAX 46
#define _SC_BARRIERS 47
#define _SC_CLOCK_SELECTION 48
#define _SC_CPUTIME 49
#define _SC_DELAYTIMER_MAX 50
#define _SC_IOV_MAX 51
#define _SC_IPV6 52
#define _SC_MAPPED_FILES 53
#define _SC_MEMLOCK 54
#define _SC_MEMLOCK_RANGE 55
#define _SC_MEMORY_PROTECTION 56
#define _SC_MESSAGE_PASSING 57
#define _SC_MQ_OPEN_MAX 58
#define _SC_MQ_PRIO_MAX 59
#define _SC_PRIORITIZED_IO 60
#define _SC_PRIORITY_SCHEDULING 61
#define _SC_RAW_SOCKETS 62
#define _SC_READER_WRITER_LOCKS 63
#define _SC_REALTIME_SIGNALS 64
#define _SC_REGEXP 65
#define _SC_RTSIG_MAX 66
#define _SC_SEMAPHORES 67
#define _SC_SHARED_MEMORY_OBJECTS 68
#define _SC_SHELL 69
#define _SC_SIGQUEUE_MAX 70
#define _SC_SPAWN 71
#define _SC_SPIN_LOCKS 72
#define _SC_SPORADIC_SERVER 73
#define _SC_SS_REPL_MAX 74
#define _SC_SYNCHRONIZED_IO 75
#define _SC_SYMLOOP_MAX 76
#define _SC_THREAD_ATTR_STACKADDR 77
#define _SC_THREAD_ATTR_STACKSIZE 78
#define _SC_THREAD_CPUTIME 79
#define _SC_THREAD_DESTRUCTOR_ITERATIONS 80
#define _SC_THREAD_KEYS_MAX 81
#define _SC_THREAD_PRIO_INHERIT 82
#define _SC_THREAD_PRIO_PROTECT 83
#define _SC_THREAD_PRIORITY_SCHEDULING 84
#define _SC_THREAD_PROCESS_SHARED 85
#define _SC_THREAD_ROBUST_PRIO_INHERIT 86
#define _SC_THREAD_ROBUST_PRIO_PROTECT 87
#define _SC_THREAD_SAFE_FUNCTIONS 88
#define _SC_THREAD_SPORADIC_SERVER 89
#define _SC_THREAD_STACK_MIN 90
#define _SC_THREAD_THREADS_MAX 91
#define _SC_THREADS 92
#define _SC_TIMEOUTS 93
#define _SC_TIMER_MAX 94
#define _SC_TIMERS 95
#define _SC_TRACE 96
#define _SC_TRACE_EVENT_FILTER 97
#define _SC_TRACE_EVENT_NAME_MAX 98
#define _SC_TRACE_INHERIT 99
#define _SC_TRACE_LOG 100
#define _SC_GETGR_R_SIZE_MAX 101
#define _SC_GETPW_R_SIZE_MAX 102
#define _SC_LOGIN_NAME_MAX 103
#define _SC_THREAD_ROBUST_PRIO_INHERIT 104
#define _SC_THREAD_ROBUST_PRIO_PROTECT 105
#define _SC_TRACE_NAME_MAX 106
#define _SC_TRACE_SYS_MAX 107
#define _SC_TRACE_USER_EVENT_MAX 108
#define _SC_TTY_NAME_MAX 109
#define _SC_TYPED_MEMORY_OBJECTS 110
#define _SC_V6_ILP32_OFF32 111
#define _SC_V6_ILP32_OFFBIG 112
#define _SC_V6_LP64_OFF64 113
#define _SC_V6_LPBIG_OFFBIG 114
#define _SC_V7_ILP32_OFF32 115
#define _SC_V7_ILP32_OFFBIG 116
#define _SC_V7_LP64_OFF64 117
#define _SC_V7_LPBIG_OFFBIG 118
#define _SC_XOPEN_CRYPT 119
#define _SC_XOPEN_ENH_I18N 120
#define _SC_XOPEN_LEGACY 121
#define _SC_XOPEN_REALTIME 122
#define _SC_XOPEN_REALTIME_THREADS 123
#define _SC_XOPEN_STREAMS 124
#define _SC_XOPEN_UNIX 125
#define _SC_XOPEN_VERSION 126
#define _SC_PHYS_PAGES 500
#define _SC_AVPHYS_PAGES 501
#define _SC_NPROCESSORS_CONF 502
#define _SC_NPROCESSORS_ONLN 503

#if __BSD_VISIBLE
#define _SC_NPROCESSORS 503 /* OpenBSD extension */
#endif

#define STDIN_FILENO 0  /* standard input file descriptor */
#define STDOUT_FILENO 1 /* standard output file descriptor */
#define STDERR_FILENO 2 /* standard error file descriptor */

/* NULL already defined in sys/types.h for !_KERNEL */

#endif /* !_SYS_UNISTD_H_ */