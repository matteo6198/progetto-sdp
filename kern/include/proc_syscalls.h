#ifndef __PROC_SYSCALLS_H__
#define __PROC_SYSCALLS_H__

#include <types.h>
#include <lib.h>
#include <addrspace.h>
#include <thread.h>
#include <proc.h>
#include <current.h>
#include <machine/trapframe.h>

#include "opt-proc_syscall.h"
#include <opt-proc_manage.h>

#if OPT_PROC_SYSCALL
void sys__exit(int status);
#endif

#if OPT_PROC_MANAGE
pid_t sys_waitpid(pid_t pid, int* status, int options);
pid_t sys_getpid(void);
pid_t sys_getppid(void);
pid_t sys_fork(struct trapframe* tf);
#endif
#endif
