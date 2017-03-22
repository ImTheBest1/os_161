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

	child_proc->status = true;


	for(pid_t c_pid = PID_MIN; c_pid < PID_SIZE; c_pid++){
		if(whole_proc_table[c_pid] == NULL){
			child_proc->pid = c_pid;
			whole_proc_table[child_proc->pid] = child_proc;
			break;
		}
	}

	*retval = child_proc->pid;

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

int sys_waitpid(pid_t pid, int *status, int options, int* retval)
{

	if(pid < PID_MIN || pid > PID_SIZE){
		return ESRCH;
	}

	if(options != 0){
		return EINVAL;
	}

	struct proc *child_proc = whole_proc_table[pid];
	// child_proc cantbe NULL
	if(child_proc == NULL){
		return ESRCH;
	}

	pid_t parent_pid = child_proc->ppid;

	if(parent_pid != curproc->pid){
		// not the same parent
		return ECHILD;
	}
	if(pid == curproc->pid){
		//parent can != child
		return ECHILD;
	}

	lock_acquire(child_proc->proc_lk);
	if(child_proc->status){
		if(child_proc->proc_exit_signal == false){
			cv_wait(child_proc->proc_cv,child_proc->proc_lk);
		}
	}
	copyout(&child_proc->proc_exit_code, (userptr_t) status, sizeof(int));

	lock_release(child_proc->proc_lk);

	*retval = pid;

	// if child is already exit, then can destroy child, proc_exit_destroy somehow doesnt work
	proc_exit_destroy(child_proc);
	kfree(whole_proc_table[pid]);
	whole_proc_table[pid] = NULL;

	return 0;
	}

void sys__exit(int exitcode)
{
	pid_t parent_pid = curproc->ppid;
	pid_t pid = curproc->pid;

	curproc->proc_exit_code = _MKWAIT_EXIT(exitcode);
	curproc->proc_exit_signal = true;

	if(parent_pid != 0){
	lock_acquire(curproc->proc_lk);
	cv_broadcast(curproc->proc_cv,curproc->proc_lk);
	lock_release(curproc->proc_lk);
	}else{
	//before exit, clean the proc, proc_destroy doesnt work somehows
	proc_exit_destroy(curproc);
	kfree(whole_proc_table[pid]);
	whole_proc_table[pid] = NULL;
	}

 	thread_exit();
	//kprintf("fail sys_exit...");

}
