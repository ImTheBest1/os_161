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

int sys_open(userptr_t user_filename, int flags){
	(void) user_filename;
	(void) flags;
  size_t size;
	struct vnode *vn;
	char *filename;
	bool empty = false;
  int counter = 3 ;
	int open_code;
	filename = (char *)kmalloc(sizeof(char)*PATH_MAX);
	copyinstr(user_filename,filename,PATH_MAX, &size);
	if(user_filename == NULL){
		// no such file, non arg
		return  EFAULT;
	}
	while(!empty || counter < FILE_SIZE){
		if(curthread->filetable[counter] == NULL){
			empty = true;
		}
		counter++;
	}
	if (!empty && counter == FILE_SIZE){
		// file table is full
		return ENFILE;
	}
	open_code = vfs_open(filename,flags,0,&vn);
	if(open_code){
		return open_code;
	}
	curthread->filetable[counter]->file_vn = vn;
	curthread->filetable[counter]->flag = flags;
	curthread->filetable[counter]->offset = 0;
	curthread->filetable[counter]->file_lock = rwlock_create(filename);
	kfree(filename);
	return counter;
}
int sys_read(int fd, void *buf,size_t buflen){
	(void) fd;
	(void) buf;
	(void) buflen;
	return 0;
}
int sys_write(int fd, const void *buf, size_t buflen){
	(void) fd;
	(void) buf;
	(void) buflen;
	// check wheter the fd is Invalid
	if(fd < 0 || fd > FILE_SIZE){
		return EBADF;
	}
	if(curthread->filetable[fd] == NULL){
		// not exist
		return EBADF;
	}
	if(curthread->filetable[fd]->flag != O_RDWR ||curthread->filetable[fd]->flag != O_WRONLY){
		return EBADF;
	}
	//check the address of buf pointer

	rwlock_acquire_write(curthread->filetable[fd]->file_lock);
  struct uio writer;
	struct iovec vec;
	size_t size;
	char *bufferName = (char *) kmalloc(buflen);
	copyinstr((userptr_t)buf,bufferName,strlen(bufferName), &size);
	if(bufferName == NULL){
		return EFAULT;
	}
	uio_kinit(&vec,&writer,(void *)bufferName,buflen,curthread->filetable[fd]->offset,UIO_WRITE);
	int write_code;
	write_code = VOP_WRITE(curthread->filetable[fd]->file_vn,&writer);
	if(write_code){
		kfree(bufferName);
		rwlock_release_write(curthread->filetable[fd]->file_lock);
		return write_code;
	}
	curthread->filetable[fd]->offset = writer.uio_offset;
  rwlock_release_write(curthread->filetable[fd]->file_lock);
	int sig = buflen - writer.uio_resid;
	return sig;
}
