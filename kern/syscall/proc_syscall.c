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
	struct trapframe *child_tf;

	int err;
	child_proc = proc_create_fork("child");
	if(child_proc == NULL){
		return ENOMEM;
	}

	// copy whole file_table to child_proc
	for(int index = 0; index < FILE_SIZE; index++){
		child_proc->filetable[index] = curproc->filetable[index];
	}
	child_proc->ppid = curproc->pid;

	child_tf = kmalloc(sizeof(struct trapframe));
	if(child_tf == NULL){
		proc_destroy(child_proc);
		return ENOMEM;
	}
	memcpy(child_tf,tf,sizeof(struct trapframe));

	err = as_copy(curproc->p_addrspace, &child_proc->p_addrspace);
	if(err){
		kfree(child_tf);
		proc_destroy(child_proc);
		return ENOMEM;
	}

	err = thread_fork("child",child_proc,(void *)into_forked_process,child_tf, 0);
	if(err){
		kfree(child_tf);
		proc_destroy(child_proc);
		return err;
	}

	*retval = curproc->pid;
	for(pid_t c_pid = PID_MIN; c_pid < PID_SIZE; c_pid++){
		if(whole_proc_table[c_pid] == NULL){
			child_proc->pid = c_pid;
			whole_proc_table[child_proc->pid] = child_proc;
			break;
		}
	}

	return 0;
}


void *into_forked_process(struct trapframe *data_1, unsigned long data_2)
{
	(void) data_2;
	struct trapframe tf;
	tf = *data_1;
	tf.tf_v0 = 0;
	tf.tf_a3 = 0;
	tf.tf_epc += 4;
	as_activate();

	mips_usermode(&tf);
}

pid_t sys_getpid(int *retval){
	*retval = curproc->pid;
	return 0;
}

pid_t sys_waitpid(pid_t pid, int *status, int options, int* retval)
{
	int adr_check;

	if(pid < PID_MIN || pid > PID_SIZE){
		//kprintf("1.. sys_waitpid(), fail, pid too small or too large  ");
		*retval = -1;
		return ESRCH;
	}

	if(status == NULL){
		*retval = -1;
		return EFAULT;
	}
	//kprintf("2.. sys_waitpid(), status exits, succeed  ");

	if(options != 0 && options != WNOHANG && options != WUNTRACED){
		kprintf("3.. sys_waitpid(), options is not zero, fail  ");
		*retval = -1;
		return EINVAL;
	}
	//kprintf("3.. sys_waitpid(), options==0, succeed  ");

	struct proc *child_proc = whole_proc_table[pid];
	(void) child_proc;
	// child_proc cantbe NULL
	if(child_proc == NULL){
		//kprintf("4.. sys_waitpid(), no child_proc in proc_table, fail ");
		*retval = -1;
		return ESRCH;
	}
	//kprintf("5.. sys_waitpid(), get child_proc from proc_table, succeed  ");

	pid_t parent_pid = child_proc->ppid;
	if(parent_pid == curproc->pid){
		//kprintf("6.. sys_waitpid(), no parent, fail ");
		*retval = -1;
		return ECHILD;
	}
	//kprintf("6.. sys_waitpid(), I have parent, succeed   ");

	if(child_proc->proc_exit_signal){
		cv_wait(child_proc->proc_cv, child_proc->proc_lk);
		// wchan_sleep(child_proc->proc_wchan, &child_proc->p_lock);
		//  kprintf("7.. sys_waitpid, child_proc wait to exit, succeed  ");
	}else{
		if( options == WNOHANG){
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


	proc_destroy(child_proc); // Destroy child
	whole_proc_table[pid] = NULL;

	//kprintf("\n------------------sys_waitpid(), ends, succeed \n\n ");
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
