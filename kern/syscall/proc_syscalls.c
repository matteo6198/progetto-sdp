#include <proc_syscalls.h>
#include <opt-proc_manage.h>
#include <synch.h>
#include <syscall.h>
#include <kern/errno.h>

void sys__exit(int code){
  struct thread *actual_thread = curthread;
  struct proc *actual_proc = actual_thread->t_proc;
  KASSERT(actual_proc != NULL);
#if !OPT_PROC_MANAGE
  struct addrspace* actual_as = proc_getas();
  KASSERT(actual_as != NULL);
#endif
  /* save exit satus of the process and thread */
  actual_proc->exit_status = code;
  actual_thread->exit_status = code;
#if OPT_PROC_MANAGE
  /* detach thread */
  proc_remthread(actual_thread);
  /* signal exit */
  V(actual_proc->ended_sem);
#else
  /* destroy address space */
  as_destroy(actual_as);
#endif
  /* exit from the thread (it does not destroy it 
   * but it will be after the switch */
  thread_exit();


}


#if OPT_PROC_MANAGE
pid_t sys_waitpid(pid_t pid, int* status, int options){
  struct proc* proc;
  /* get the process from table by pid*/
  proc = get_proc_by_pid(pid);
  if(proc == NULL)
    return ECHILD;
  /* wait for process termination and status setting*/
  *status = proc_wait(proc);
  (void)options;
  return pid;
}

pid_t sys_getpid(void){
  return curproc->pid;
}
pid_t sys_getppid(void){
  return curproc->parent->pid;
}
static void enter_forked_process_wrapper(void* tf, unsigned long i){
  enter_forked_process((struct trapframe*)tf);
  (void)i;
}
pid_t sys_fork(struct trapframe* tf){
  struct proc* new_proc;
  int result;
  pid_t pid;
  struct trapframe* child_tf;
  /* create the new proc structure and add to processes table */
  new_proc = proc_dup(curproc);
  if(new_proc == NULL)
    return -ENOMEM;
  pid = new_proc->pid;

  child_tf = kmalloc(sizeof(struct trapframe));
  if(child_tf == NULL){
    proc_destroy(new_proc);
    return -ENOMEM;
  }
  memcpy(child_tf, tf, sizeof(struct trapframe));
  
  result = thread_fork(curthread->t_name, new_proc, enter_forked_process_wrapper, child_tf, 1);

  if(result){
    proc_destroy(new_proc);
    kfree(child_tf);
    return -ENOMEM;
  }
  
  return pid;
}
#endif
