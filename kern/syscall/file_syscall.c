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

int sys_open(userptr_t user_filename, int flags, mode_t mode){
	(void) user_filename;
	(void) flags;
	(void) mode;

	char *filename = (char*) user_filename;
	struct vnode *vn;

 //  	size_t size;
	// struct vnode *vn;

	// copy user_filename to filename


	bool empty = false;
  	int index = 3 ; // 0,1,2 for reserve spot
	int open_code;
	// filename = (char *)kmalloc(sizeof(char)*PATH_MAX);
	// copyinstr(user_filename,filename,PATH_MAX, &size);
	copyin((const_userptr_t)user_filename,filename,sizeof(filename));

	if(user_filename == NULL){
		// no such file, non arg
		return  EFAULT;
		empty = true;
	}

	// check if file table is available for later file_descriptor pointer to file objector, save spot for open
	while(!empty || index < FILE_SIZE){
		if(curproc->filetable[index] == NULL){
			empty = true;
			// as long as it is empty, break;
			break;
		}
		index++;
	}

	if (!empty && index == FILE_SIZE){
		// file table is full
		return ENFILE;
	}
	// vfs_open(char *path, int openflags, mode_t mode, struct vnode **ret);
	// open_code = vfs_open(filename,flags,0,&vn);
	open_code = vfs_open(filename,flags,mode,&vn);
	if(open_code){
		return open_code;
	}
	curproc->filetable[index]->file_vn = vn;
	curproc->filetable[index]->flag = flags;
	curproc->filetable[index]->offset = 0;
	curproc->filetable[index]->file_lock = rwlock_create(filename);
	// kfree(filename);

	return index;
}


int sys_read(int fd, void *buf,size_t buflen){
	(void) fd;
	(void) buf;
	(void) buflen;
	return 0;
}

ssize_t sys_write(int fd, const void *buf, size_t buflen){
	(void) fd;
	(void) buf;
	(void) buflen;
	// check wheter the fd is Invalid
	if(fd < 0 || fd > FILE_SIZE){
		return EBADF;
	}
	if(curproc->filetable[fd] == NULL){
		// not exist
		return EBADF;
	}
	if(curproc->filetable[fd]->flag != O_RDWR ||curproc->filetable[fd]->flag != O_WRONLY){
		return EBADF;
	}
	//check the address of buf pointer

	rwlock_acquire_write(curproc->filetable[fd]->file_lock);
  	struct uio writer;
	struct iovec vec;
	size_t size;
	char *bufferName = (char *) kmalloc(buflen);
	copyinstr((userptr_t)buf,bufferName,strlen(bufferName), &size);
	if(bufferName == NULL){
		return EFAULT;
	}
	uio_kinit(&vec,&writer,(void *)bufferName,buflen,curproc->filetable[fd]->offset,UIO_WRITE);
	int write_code;
	write_code = VOP_WRITE(curproc->filetable[fd]->file_vn,&writer);
	if(write_code){
		kfree(bufferName);
		rwlock_release_write(curproc->filetable[fd]->file_lock);
		return write_code;
	}
	curproc->filetable[fd]->offset = writer.uio_offset;
  rwlock_release_write(curproc->filetable[fd]->file_lock);
	int sig = buflen - writer.uio_resid;
	return sig;
}
