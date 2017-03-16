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
  // kprintf("check 5\n");
  *retval = curproc->pid;
     return 0;
 }

int sys_getpid(pid_t *retval){
   *retval = curproc->pid;
   return 0;
}
int sys_execv(const char *program,char **args, int *retval){
  (void)program;
  (void)args;
  (void)retval;
  struct vnode *vn;
  struct addrspace *old_addr;
  struct addrspace *new_addr;
  vaddr_t ep,stack;
  int error;
  char *filename;
  int d = 0;
  filename = (char *)kmalloc(sizeof(PATH_MAX));
  error = copyin((userptr_t)program,filename,sizeof(filename));
  kprintf("check 1\n");
  if(error){
    *retval = -1;
    return EFAULT;
  }
  // count args
  int count =0;
  while(args[count] != NULL){
    count++;
  }
  //copy args
  char **argholder;
  kprintf("check 1.5\n");
  argholder = (char **)kmalloc(sizeof(char*)*count);
  for(int i =0; i < count;i++){
    argholder[i] = (char *)kmalloc(strlen(args[i])+1);
    if(  argholder[i] == NULL){
      int j = 0;
        while( argholder[j] != NULL){
          kfree(argholder[j]);
        }
        kfree(filename);
        kfree(argholder);
        *retval = -1;
        return ENOMEM;
    }
    kprintf("check 2\n");
    error = copyin((const_userptr_t)&(args[i]),&argholder[i],sizeof(userptr_t));
    if(error){
        while( argholder[d] != NULL){
          kfree(argholder[d]);
        }
        kfree(filename);
        kfree(argholder);
       *retval = -1;
      return error;
    }
}
    argholder[count] = NULL;
    kprintf("check 2.5\n");
    // files open here
    error = vfs_open(filename, O_RDONLY,0,&vn);
    if(error){
            kprintf("err 0\n");
        d = 0;
        while( argholder[d] != NULL){
          kfree(argholder[d]);
        }
        kfree(filename);
        kfree(argholder);
       *retval = -1;
      return error;
    }
    // start to copy addr
  new_addr =    as_create();
  old_addr = proc_setas(new_addr);
  as_activate();
  error = load_elf(vn,&ep);
  if(error){
    kprintf("err 1: %d\n",error);
    d = 0;
    while( argholder[d] != NULL){
      kfree(argholder[d]);
    }
    kfree(filename);
    kfree(argholder);
    as_deactivate();
    as_destroy(proc_setas(old_addr));
    vfs_close(vn);
   *retval = -1;
  return error;
  }
  vfs_close(vn);

    kprintf("check 3\n");
  //stack copy
  error = as_define_stack(curproc->p_addrspace,&stack);
  if(error){
    d = 0;
    while( argholder[d] != NULL){
      kfree(argholder[d]);
    }
    kfree(filename);
    kfree(argholder);
    as_deactivate();
    as_destroy(proc_setas(old_addr));
    vfs_close(vn);
   *retval = -1;
  return error;
  }

  unsigned int arg_addr[count];
  for(int i =count - 1; i >= 0;i--){
    int shift =  (strlen(argholder[i]))%4 == 0? 4: (strlen(argholder[i]))%4;
    shift = shift + strlen( argholder[i]);
    stack = stack - shift;
    error = copyout(argholder[i],(userptr_t)&stack,strlen( argholder[i]+1));
    if(error){
      d = 0;
      while( argholder[d] != NULL){
        kfree(argholder[d]);
      }
      kfree(arg_addr);
      kfree(filename);
      kfree(argholder);
      as_deactivate();
      as_destroy(proc_setas(old_addr));
      vfs_close(vn);
     *retval = -1;
    return error;
    }
    arg_addr[i] = stack;
   }
       kprintf("check 4\n");
    arg_addr[count] = (int)NULL;
    for(int i =count - 1; i >= 0;i--){
       stack -= 4;
       error = copyout(&arg_addr[i],(userptr_t)&stack,strlen(argholder[i]+1));
       if(error){
         d = 0;
         while( argholder[d] != NULL){
           kfree(argholder[d]);
         }
         kfree(arg_addr);
         kfree(filename);
         kfree(argholder);
         as_deactivate();
         as_destroy(proc_setas(old_addr));
         vfs_close(vn);
        *retval = -1;
       return error;
       }
    }
        kprintf("check 5\n");
    *retval = 0;
    enter_new_process(count,(userptr_t)stack,NULL,stack,ep);

    // this shoule not run
    panic("execv didn't enter new process....\n");

    return EINVAL;
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
