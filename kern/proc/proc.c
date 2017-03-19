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
#include <kern/fcntl.h>
#include <vfs.h>
#include <syscall.h>
#include <synch.h>
#include <limits.h>
#include <wchan.h>
#include <syscall.h>


/*
* The process for the kernel; this holds all the kernel-only threads.
*/
struct proc *kproc;
struct proc *whole_proc_table[PID_SIZE];


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

	proc->filetable = kmalloc(FILE_SIZE * (sizeof(struct file_handler)));

	proc->p_numthreads = 0;
	spinlock_init(&proc->p_lock);

	/* VM fields */
	proc->p_addrspace = NULL;

	/* VFS fields */
	proc->p_cwd = NULL;

	for(int i = 0; i < FILE_SIZE;i++){
		proc->filetable[i] = NULL;
	}

	proc->ppid = 0; // default for kernel, i guess

	// initialize process table
	for(pid_t id = PID_MIN; id < PID_SIZE; ++id){
		if(whole_proc_table[id] ==  NULL){
			proc->pid = id;
			whole_proc_table[proc->pid] = proc;
			break;
		}
	}

	proc->proc_exit_code = 0;
	proc->proc_cv = cv_create("proc_cv");
	proc->proc_lk = lock_create("proc_lk");
	proc->proc_exit_signal = false;

	return proc;
}



// -------DEAD---------------------
// have the issue that make system stop working
void file_handler_std_init(struct proc *cur_proc){
	char deceive_console[] = "con:";

	int sig;
	struct vnode *vn_1 = NULL;
	struct vnode *vn_2 = NULL;
	int sig_1;
	struct vnode *vn_3 = NULL;
	int sig_2;



	cur_proc->filetable[0] = kmalloc(sizeof(struct file_handler));
	sig = vfs_open(deceive_console,O_RDONLY,0,&vn_1); // or mode_t = 0
	(void) sig;
	cur_proc->filetable[0]->file_vn = vn_1;
	cur_proc->filetable[0]->flag = O_RDONLY;
	cur_proc->filetable[0]->offset = 0;
	cur_proc->filetable[0]->file_lk = lock_create(deceive_console);

	cur_proc->filetable[1] = kmalloc(sizeof(struct file_handler));
	sig_1 =  vfs_open(deceive_console,O_WRONLY,0,&vn_2); // or mode_t = 0
	(void) sig_1;

	cur_proc->filetable[1] ->file_vn = vn_2;
	cur_proc->filetable[1] ->flag = O_WRONLY;
	cur_proc->filetable[1] ->offset = 0;
	cur_proc->filetable[1]->file_lk = lock_create(deceive_console);

	cur_proc->filetable[2] = kmalloc(sizeof(struct file_handler));
	sig_2 =  vfs_open(deceive_console,O_WRONLY,0,&vn_3); // or mode_t = 0
	(void) sig_2;

	cur_proc->filetable[2] ->file_vn = vn_3;
	cur_proc->filetable[2] ->flag = (int)O_WRONLY;
	cur_proc->filetable[2] ->offset = 0;
	cur_proc->filetable[2]->file_lk = lock_create(deceive_console);


}

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


	for (int index = 0; index < FILE_SIZE; ++index){
		if (proc->filetable[index] != NULL){
			if (proc->filetable[index]->file_vn) {
				VOP_DECREF(proc->filetable[index]->file_vn);
				vfs_close(proc->filetable[index]->file_vn);
				proc->filetable[index]->file_vn = NULL;
			}
			lock_destroy(proc->filetable[index]->file_lk); // destroy lock inside file_table
			proc->filetable[index] = NULL; // kfree
		}
	}
	cv_destroy(proc->proc_cv);
	lock_destroy(proc->proc_lk);

	proc->p_numthreads = 0;
	KASSERT(proc->p_numthreads == 0);
	spinlock_cleanup(&proc->p_lock);

	kfree(proc->p_name);
	kfree(proc);

}


/*
* Create the process structure for the kernel.
*/
void
proc_bootstrap(void)
{
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

	/* VFS fields */
	newproc->ppid = PID_MIN; // thats after the proc_create, set default to 1

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

	//file_handler_std_init(newproc);
	return newproc;
}

struct proc *proc_create_fork(const char *name, struct proc *cur_proc, int *retval){
	struct proc *child_proc;
	child_proc = proc_create_runprogram(name);
	if(child_proc == NULL){
		proc_destroy(child_proc);
		*retval = -1;
		return NULL;
	}
	child_proc->ppid = cur_proc->pid; // current pid is child_proc's parent pid
	if(cur_proc->pid == -1){
		kprintf("cur_proc pid < 0");
	}else{
		kprintf("cur_proc pid = %d\n", cur_proc->pid);
	}
	// copy whole file_table to child_proc
	for(int index = 0; index < FILE_SIZE; index++){
		child_proc->filetable[index] = curproc->filetable[index];
	}

	return child_proc;
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
