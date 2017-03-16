#include <types.h>
#include <copyinout.h>
#include <syscall.h>
#include <kern/errno.h>
#include <limits.h>
#include <current.h>
#include <thread.h>
#include <lib.h>
#include <vfs.h>
#include <synch.h>
#include <uio.h>
#include <kern/fcntl.h>
#include <vnode.h>
#include <proc.h>
#include <kern/errno.h>
#include <kern/seek.h>
#include <kern/stat.h>
#include <endian.h>
#include <mips/trapframe.h>
#include <addrspace.h>
#include <kern/wait.h>



int sys_fork(struct trapframe *tf,pid_t *retval){

	struct proc *child_proc;
	(void) child_proc;
	struct trapframe *child_tf;
	(void) child_tf;
	struct addrspace *child_addrspace;
	(void) child_addrspace;

	int err;

	// step1: create child proc
    child_proc = proc_create_fork("child", curproc, retval);

	// step2: get child addrspace and trapframe
		// step2.1 get child_tf
		child_tf = kmalloc(sizeof(struct trapframe));
		if(child_tf == NULL){
    		kfree(child_tf);
    		proc_destroy(child_proc);
			*retval = -1;
    		return ENOMEM;
  		}
  		memcpy(child_tf,tf,sizeof(struct trapframe));

		// step 2.2 get child_addrspace
		child_addrspace = kmalloc(sizeof(struct addrspace));
		//as_copy(struct addrspace *src, struct addrspace **ret);
		err = as_copy(curproc->p_addrspace, &child_proc->p_addrspace);
		if(err){
	      kfree(child_addrspace);
		  kfree(child_tf);
	      proc_destroy(child_proc);
		  *retval = -1;
	      return ENOMEM;
	    }

	// step3 : make a new thread
	//int thread_fork(const char *name, struct proc *proc, void (*func)(void *, unsigned long), void *data1, unsigned long data2);
	void **package = kmalloc(2*sizeof(void *));
	package[0] = (void *)child_proc->p_addrspace;
	package[1] = (void *)child_tf;
	err = thread_fork("child",child_proc,&into_forked_process,package,0);
  	if(err){
    	kfree(child_addrspace);
    	kfree(child_tf);
    	proc_destroy(child_proc);
    	return err;
  	}

  *retval = curproc->pid;
  return 0;
}


void into_forked_process(void *data_1,unsigned long data_2)
{
	  (void)data_2;

    struct trapframe *tf = ((void **)data_1)[1];
    struct addrspace *child_addrspace = ((void **)data_1)[0];
    struct trapframe tf_1;
    tf->tf_v0 = 0;
    tf->tf_a3 = 0;
    tf->tf_epc += 4;

    curproc->p_addrspace = child_addrspace;
    as_activate();

    memcpy(&tf_1, tf, sizeof(struct trapframe));
	mips_usermode(&tf_1);
}

pid_t sys_getpid(void){
	  return curproc->pid;
}

pid_t sys_waitpid(pid_t pid, int *status, int options, int* retval)
{
	(void) status;
	(void) options;
	(void) retval;
	int adr_check;
	(void) adr_check;
	if(pid < PID_MIN || pid > PID_SIZE){
		*retval = -1;
		return ESRCH;
	}
	if(status == NULL){
		*retval = -1;
		return EFAULT;
	}

	if(options != 0 || options != WNOHANG || options != WUNTRACED){
        *retval = -1;
        return EINVAL;
    }

	struct proc *child_proc = whole_proc_table[pid];
	(void) child_proc;
	// child_proc cantbe NULL
	if(child_proc == NULL){
		*retval = -1;
		return ESRCH;
	}
	pid_t parent_pid = child_proc->ppid;
	if(parent_pid != curproc->pid){
		*retval = -1;
		return ECHILD;
	}

	if(child_proc->proc_exit_signal){
		cv_wait(child_proc->proc_cv, child_proc->proc_lk);
	}else{
		if( options == WNOHANG || options == WUNTRACED ){
			*retval = -1;
			return 0;
		}
	}

	// *status = child_proc->proc_exit_code;
	adr_check = copyout(&child_proc->proc_exit_code, (userptr_t) status, sizeof(int));
	if(adr_check){
		*retval = -1;
		return adr_check;
	}

	whole_proc_table[pid] = NULL;
	proc_destroy(child_proc); // Destroy child

	return pid;
}

void sys__exit(int exitcode){
	(void) exitcode;

	lock_acquire(curproc->proc_lk);
	pid_t parent_pid = curproc->ppid;
	(void) parent_pid;

	if(whole_proc_table[parent_pid]->proc_exit_signal == false){
		curproc->proc_exit_signal = true;
		curproc->proc_exit_code = _MKWAIT_EXIT(exitcode);
		cv_broadcast(curproc->proc_cv, curproc->proc_lk);
		lock_release(curproc->proc_lk);
	}
	else{

		proc_destroy(curproc);
	}

	thread_exit();

}
