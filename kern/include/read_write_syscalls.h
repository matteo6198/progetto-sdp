#ifndef __READ_WRITE_SYSCALLS__
#define __READ_WRITE_SYSCALLS__

#include "opt-read_write.h"
#include <types.h>
#include <lib.h>
#include <limits.h>
#include <vnode.h>
#include <vfs.h>
#include <spinlock.h>
#include <current.h>
#include <proc.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <uio.h>
#include <copyinout.h>

#define SYS_OPEN_FILE_MAX 10*OPEN_MAX

#if OPT_READ_WRITE
int sys_read(int file, void* buffer, int size);
int sys_write(int file, void* buffer, int size);
int sys_open(char* filename, int flags);
int sys_close(int fd);
#endif

#endif
