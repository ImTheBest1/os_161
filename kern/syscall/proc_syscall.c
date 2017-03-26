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
		if(child_proc->filetable[index] != NULL){
			child_proc->filetable[index]->file_reference_count++;
		}
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
	// proc_exit_destroy(child_proc);
	// kfree(whole_proc_table[pid]);
	// whole_proc_table[pid] = NULL;

	return 0;
	}

void sys__exit(int exitcode)
{
	pid_t parent_pid = curproc->ppid;
	pid_t pid = curproc->pid;
	(void) pid;

	curproc->proc_exit_code = _MKWAIT_EXIT(exitcode);
	curproc->proc_exit_signal = true;

	if(parent_pid != 0){
	lock_acquire(curproc->proc_lk);
	cv_broadcast(curproc->proc_cv,curproc->proc_lk);
	lock_release(curproc->proc_lk);
	}else{
	//before exit, clean the proc, proc_destroy doesnt work somehows
	// proc_exit_destroy(curproc);
	// kfree(whole_proc_table[pid]);
	// whole_proc_table[pid] = NULL;
	}

 	thread_exit();
}

int sys_execv(const char *program, char **args, int *retval){
	(void) args;
	(void) retval;
	struct addrspace *as;
	struct vnode *v;
	vaddr_t entrypoint, stackptr;
	int result;

	//check if program != NULL
	if(program == NULL){
		return EFAULT;
	}

	// copy user space to kernel space
	char *kernel_program = (char *)kmalloc(sizeof(char)*PATH_MAX);
	result = copyin((const_userptr_t)program, kernel_program,PATH_MAX);

	if(result){
		kprintf("copyin userspace program fail\n");
		kfree(kernel_program);
		return result;
	}

	if(strlen(kernel_program) == 0){
		return -1;
	}

	// get the args array size
	int args_size = 0;
	while(args_size < ARG_MAX){
		if(args[args_size] == NULL){
			break;
		}
		args_size++;
	}

	if (args_size == 0 || args == NULL){
		return -1;
	}

	char **kernel_args = (char **) kmalloc(sizeof(char *) * args_size);
	result = copyin((const_userptr_t) kernel_args, args, sizeof(args));

	/* Open the file. */
	result = vfs_open(kernel_program, O_RDONLY, 0, &v);
	if (result) {
		return result;
	}

	// destroy current addrspace
	if(curproc->p_addrspace != NULL){
		as_destroy(curproc->p_addrspace);
		curproc->p_addrspace = NULL;
	}

	/* We should be a new process. */
	KASSERT(proc_getas() == NULL);

	/* Create a new address space. */
	as = as_create();
	if (as == NULL) {
		vfs_close(v);
		return ENOMEM;
	}

	/* Switch to it and activate it. */
	proc_setas(as);
	as_activate();

	/* Load the executable. */
	result = load_elf(v, &entrypoint);
	if (result) {
		/* p_addrspace will go away when curproc is destroyed */
		vfs_close(v);
		return result;
	}

	/* Done with the file now. */
	vfs_close(v);

	/* Define the user stack in the address space */
	result = as_define_stack(as, &stackptr);
	if (result) {
		/* p_addrspace will go away when curproc is destroyed */
		return result;
	}

	/* Warp to user mode. */
	enter_new_process(0 /*argc*/, NULL /*userspace addr of argv*/,
			  NULL /*userspace addr of environment*/,
			  stackptr, entrypoint);

	/* enter_new_process does not return. */
	panic("enter_new_process returned\n");
	return EINVAL;





















	return 0;
}
