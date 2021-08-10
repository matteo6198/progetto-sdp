/*
 * Copyright (c) 2013
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Process support.
 *
 * There is (intentionally) not much here; you will need to add stuff
 * and maybe change around what's already present.
 *
 * p_lock is intended to be held when manipulating the pointers in the
 * proc structure, not while doing any significant work with the
 * things they point to. Rearrange this (and/or change it to be a
 * regular lock) as needed.
 *
 * Unless you're implementing multithreaded user processes, the only
 * process that will have more than one thread is the kernel process.
 */

#include <types.h>
#include <spl.h>
#include <proc.h>
#include <current.h>
#include <addrspace.h>
#include <vnode.h>
#include <opt-proc_manage.h>
#define MAX_PROC 100
/*
 * The process for the kernel; this holds all the kernel-only threads.
 */
struct proc *kproc;

#if OPT_PROC_MANAGE
static struct spinlock process_table_lock=SPINLOCK_INITIALIZER;
struct proc* processes[MAX_PROC];
static int last_pid=-1;

static int add_proc(struct proc* p){
  int start;
  int assigned = 0;
  spinlock_acquire(&process_table_lock);
  start = (last_pid + 1) % MAX_PROC;
  while(start != last_pid){
    if(processes[start] == NULL){
      processes[start] = p;
      p->pid = (pid_t)start;
      assigned = 1;
      last_pid = start;
    }else
      start = (start + 1) % MAX_PROC;
  }
  spinlock_release(&process_table_lock);
  return assigned;
}

static void remove_proc(pid_t pid){
  if(pid >= MAX_PROC || pid < 0)
    return;
  spinlock_acquire(&process_table_lock);
  processes[pid] = NULL;
  last_pid = pid - 1;
  spinlock_release(&process_table_lock);
}

struct proc* get_proc_by_pid(pid_t pid){
  if(pid >= MAX_PROC || pid < 0)
    return NULL;
  return processes[pid];
}

#endif
/*
 * Create a proc structure.
 */
static
struct proc *
proc_create(const char *name)
{
	struct proc *proc;

	proc = kmalloc(sizeof(*proc));
	if (proc == NULL) {
		return NULL;
	}
	proc->p_name = kstrdup(name);
	if (proc->p_name == NULL) {
		kfree(proc);
		return NULL;
	}
#if OPT_PROC_MANAGE
	proc->ended_sem = NULL;

	/* assign pid*/
	if(!add_proc(proc)){
	  kfree(proc->p_name);
	  kfree(proc);
	  return NULL;
	}
#endif
#if OPT_READ_WRITE
	int i;
	for(i=0;i<OPEN_MAX;i++){
	  proc->open_files[i] = NULL;
	}
#endif

	proc->p_numthreads = 0;
	spinlock_init(&proc->p_lock);

	/* VM fields */
	proc->p_addrspace = NULL;

	/* VFS fields */
	proc->p_cwd = NULL;
#if OPT_ONDEMANDE_MANAGE
	proc->p_elf = NULL;
#endif
	return proc;
}

#if OPT_PROC_MANAGE
struct proc* proc_dup(struct proc* proc){
  struct addrspace* old_addrspace;
  struct addrspace* new_addrspace;
  struct proc* new_proc;
  
  if(proc == NULL)
    return NULL;
  new_proc = proc_create(proc->p_name);
  /* addresspace copy */
  old_addrspace = proc_getas();
  if(as_copy(old_addrspace, &new_addrspace) != 0){
    proc_destroy(new_proc);
    return NULL;
  }
  new_proc->p_addrspace = new_addrspace;
  /* init the end semaphore */
  new_proc->ended_sem = sem_create("", 0);
  /* setup cwd */
  VOP_INCREF(proc->p_cwd);
  new_proc->p_cwd = proc->p_cwd;
  // copy open file table
#if OPT_READ_WRITE
  int i;
  for(i=0;i<OPEN_MAX;i++){
    new_proc->open_files[i] = proc->open_files[i];
  }
#endif
#if OPT_ONDEMANDE_MANAGE
	new_proc->p_elf = proc->p_elf;
	vnode_incref(new_proc->p_elf);
#endif
  /* setup parent */
  new_proc->parent = proc;
  return new_proc;  
}

#endif
/*
 * Destroy a proc structure.
 *
 * Note: nothing currently calls this. Your wait/exit code will
 * probably want to do so.
 */
void
proc_destroy(struct proc *proc)
{
	/*
	 * You probably want to destroy and null out much of the
	 * process (particularly the address space) at exit time if
	 * your wait/exit design calls for the process structure to
	 * hang around beyond process exit. Some wait/exit designs
	 * do, some don't.
	 */

	KASSERT(proc != NULL);
	KASSERT(proc != kproc);

	/*
	 * We don't take p_lock in here because we must have the only
	 * reference to this structure. (Otherwise it would be
	 * incorrect to destroy it.)
	 */

	/* VFS fields */
	if (proc->p_cwd) {
		VOP_DECREF(proc->p_cwd);
		proc->p_cwd = NULL;
	}
#if OPT_READ_WRITE
	// close all opened files
	int i;
	for(i=0;i<OPEN_MAX;i++){
	  if(proc->open_files[i] != NULL)
	    sys_close(i);
	}
#endif
	/* VM fields */
	if (proc->p_addrspace) {
		/*
		 * If p is the current process, remove it safely from
		 * p_addrspace before destroying it. This makes sure
		 * we don't try to activate the address space while
		 * it's being destroyed.
		 *
		 * Also explicitly deactivate, because setting the
		 * address space to NULL won't necessarily do that.
		 *
		 * (When the address space is NULL, it means the
		 * process is kernel-only; in that case it is normally
		 * ok if the MMU and MMU- related data structures
		 * still refer to the address space of the last
		 * process that had one. Then you save work if that
		 * process is the next one to run, which isn't
		 * uncommon. However, here we're going to destroy the
		 * address space, so we need to make sure that nothing
		 * in the VM system still refers to it.)
		 *
		 * The call to as_deactivate() must come after we
		 * clear the address space, or a timer interrupt might
		 * reactivate the old address space again behind our
		 * back.
		 *
		 * If p is not the current process, still remove it
		 * from p_addrspace before destroying it as a
		 * precaution. Note that if p is not the current
		 * process, in order to be here p must either have
		 * never run (e.g. cleaning up after fork failed) or
		 * have finished running and exited. It is quite
		 * incorrect to destroy the proc structure of some
		 * random other process while it's still running...
		 */
		struct addrspace *as;

		if (proc == curproc) {
			as = proc_setas(NULL);
			as_deactivate();
		}
		else {
			as = proc->p_addrspace;
			proc->p_addrspace = NULL;
		}
		as_destroy(as);
	}

	KASSERT(proc->p_numthreads == 0);
	spinlock_cleanup(&proc->p_lock);
#if OPT_PROC_MANAGE
	if(proc->ended_sem != NULL)
	  sem_destroy(proc->ended_sem);
	remove_proc(proc->pid);
#endif
#if OPT_ONDEMANDE_MANAGE
	vfs_close(proc->p_elf);
#endif
	kfree(proc->p_name);
	kfree(proc);
}

/*
 * Create the process structure for the kernel.
 */
void
proc_bootstrap(void)
{
#if OPT_PROC_MANAGE
  int i;
  for(i=0;i<MAX_PROC;i++)
    processes[i]=NULL;
#endif
	kproc = proc_create("[kernel]");
	if (kproc == NULL) {
		panic("proc_create for kproc failed\n");
	}
}

/*
 * Create a fresh proc for use by runprogram.
 *
 * It will have no address space and will inherit the current
 * process's (that is, the kernel menu's) current directory.
 */
struct proc *
proc_create_runprogram(const char *name)
{
	struct proc *newproc;

	newproc = proc_create(name);
	if (newproc == NULL) {
		return NULL;
	}

	/* VM fields */

	newproc->p_addrspace = NULL;

#if OPT_PROC_MANAGE
	/* create the semaphore needed to wait for the proc*/
	newproc->ended_sem = sem_create("", 0);
	newproc->parent = curproc;
#endif

	/* VFS fields */

	/*
	 * Lock the current process to copy its current directory.
	 * (We don't need to lock the new process, though, as we have
	 * the only reference to it.)
	 */
	spinlock_acquire(&curproc->p_lock);
	if (curproc->p_cwd != NULL) {
		VOP_INCREF(curproc->p_cwd);
		newproc->p_cwd = curproc->p_cwd;
	}
	spinlock_release(&curproc->p_lock);
	
	return newproc;
}

/*
 * Add a thread to a process. Either the thread or the process might
 * or might not be current.
 *
 * Turn off interrupts on the local cpu while changing t_proc, in
 * case it's current, to protect against the as_activate call in
 * the timer interrupt context switch, and any other implicit uses
 * of "curproc".
 */
int
proc_addthread(struct proc *proc, struct thread *t)
{
	int spl;

	KASSERT(t->t_proc == NULL);

	spinlock_acquire(&proc->p_lock);
	proc->p_numthreads++;
	spinlock_release(&proc->p_lock);

	spl = splhigh();
	t->t_proc = proc;
	splx(spl);

	return 0;
}

/*
 * Remove a thread from its process. Either the thread or the process
 * might or might not be current.
 *
 * Turn off interrupts on the local cpu while changing t_proc, in
 * case it's current, to protect against the as_activate call in
 * the timer interrupt context switch, and any other implicit uses
 * of "curproc".
 */
void
proc_remthread(struct thread *t)
{
	struct proc *proc;
	int spl;

	proc = t->t_proc;
	KASSERT(proc != NULL);

	spinlock_acquire(&proc->p_lock);
	KASSERT(proc->p_numthreads > 0);
	proc->p_numthreads--;
	spinlock_release(&proc->p_lock);

	spl = splhigh();
	t->t_proc = NULL;
	splx(spl);
}

/*
 * Fetch the address space of (the current) process.
 *
 * Caution: address spaces aren't refcounted. If you implement
 * multithreaded processes, make sure to set up a refcount scheme or
 * some other method to make this safe. Otherwise the returned address
 * space might disappear under you.
 */
struct addrspace *
proc_getas(void)
{
	struct addrspace *as;
	struct proc *proc = curproc;

	if (proc == NULL) {
		return NULL;
	}

	spinlock_acquire(&proc->p_lock);
	as = proc->p_addrspace;
	spinlock_release(&proc->p_lock);
	return as;
}

/*
 * Change the address space of (the current) process. Return the old
 * one for later restoration or disposal.
 */
struct addrspace *
proc_setas(struct addrspace *newas)
{
	struct addrspace *oldas;
	struct proc *proc = curproc;

	KASSERT(proc != NULL);

	spinlock_acquire(&proc->p_lock);
	oldas = proc->p_addrspace;
	proc->p_addrspace = newas;
	spinlock_release(&proc->p_lock);
	return oldas;
}

#if OPT_PROC_MANAGE

int proc_wait(struct proc* p){
  int status;
  KASSERT(p!=NULL);
  /* wait for process end*/
  P(p->ended_sem);
  /* get status*/
  status = p->exit_status;
  /* destroy the process */
  proc_destroy(p);
  /* return end status*/
  return status;
}
#endif

#if OPT_READ_WRITE
int proc_add_file(struct openfile* file){
  int i, fd=-1;
  spinlock_acquire(&curproc->p_lock);
  // check if already present entry
  for(i = STDERR_FILENO + 1; i < OPEN_MAX; i++){
    if(curproc->open_files[i] == file){
      fd = i;
      break;
    }
  }
  for(i = STDERR_FILENO + 1; i < OPEN_MAX && fd < 0; i++){
    if(curproc->open_files[i] == NULL){
      fd = i;
      curproc->open_files[i] = file;
      break;
    }
  }
  spinlock_release(&curproc->p_lock);
  if(i == OPEN_MAX)
    return -EMFILE;
  return fd;
}

struct openfile* proc_rem_file(int fd){
  struct openfile* file;
  if(fd >= OPEN_MAX || fd < 0){
    return NULL;
  }

  spinlock_acquire(&curproc->p_lock);
  file = curproc->open_files[fd];
  curproc->open_files[fd] = NULL;
  spinlock_release(&curproc->p_lock);
  return file;
}
#endif
