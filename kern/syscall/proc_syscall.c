#include <types.h>
#include <copyinout.h>
#include <syscall.h>
#include <kern/errno.h>
#include <limits.h>
#include <current.h>
#include <thread.h>
#include <lib.h>
#include <vfs.h>
#include <uio.h>
#include <kern/fcntl.h>
#include <vnode.h>
#include <proc.h>
#include <kern/errno.h>
#include <kern/seek.h>
#include <kern/stat.h>
#include <endian.h>
#include <addrspace.h>
#include <mips/trapframe.h>



 int sys_fork(struct trapframe *tf,pid_t *retval){
   (void)tf;
   (void)retval;
  //struct addrspace *addr;
  pid_t pid;
  // create new process
  //kprintf("check 1\n");
  struct proc *child = proc_create_runprogram(curproc->p_name);
  if(child == NULL){
    *retval = -1;
    return ENOMEM;
  }
  // copy data
  int error = 0;
  // addr = kmalloc(sizeof(struct addrspace));
  // if(addr == NULL){
  //   kfree(addr);
  //   proc_destroy(child);
  //   return error;
  // }
  //kprintf("check 2\n");
  //kprintf("returned: %d\n",curproc->p_addrspace->as_pbase1);
  error = as_copy(curproc->p_addrspace,&(child->p_addrspace));
  if(error){
    //kfree(addr);
    proc_destroy(child);
    return error;
  }
  struct trapframe *child_tf = kmalloc(sizeof(struct trapframe));
  if(child_tf == NULL){
    kfree(child_tf);
    proc_destroy(child);
    return error;
  }
    //kprintf("check 3\n");
  memcpy(child_tf,tf,sizeof(struct trapframe));
  // set ppid and pid
  pid = pid_link_acquire(child);
  child->pid = pid;
  child->ppid = curproc->pid;
  // thread_fork
	void **package = kmalloc(sizeof(struct trapframe)+sizeof(struct addrspace));
	package[0] = (void *)child->p_addrspace;
	package[1] = (void *)child_tf;
  //kprintf("check 4\n");
  error = thread_fork("child",child,helper,package,0);

  if(error){
    kfree(child_tf);

    // change pid needed here

    proc_destroy(child);
    return error;
  }
  kprintf("check 5\n");
  *retval = curproc->pid;
     return 0;
 }

int sys_getpid(pid_t *retval){
   *retval = curproc->pid;
   return 0;
}
void helper(void *data_1,unsigned long data_2){
	  (void)data_2;
    struct trapframe *tf = ((void **)data_1)[1];
    struct addrspace *addr = ((void **)data_1)[0];
    struct trapframe tf_1;
    tf->tf_v0 = 0;
    tf->tf_a3 = 0;
    tf->tf_epc += 4;

    curproc->p_addrspace = addr;
    as_activate();

    tf_1 = *tf;
    mips_usermode(&tf_1);

  }
