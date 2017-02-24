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

int sys_open(userptr_t user_filename, int flags, mode_t mode){
	(void) user_filename;
	(void) flags;
	(void) mode;

	char *filename = (char*) user_filename;
	struct vnode *vn;
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
	// kfree(filename);

	return index;
}


int sys_read(int fd, void *buf,size_t buflen){
	(void) fd;
	(void) buf;
	(void) buflen;
	return 0;
}

int sys_write(int fd, const void *buf, size_t buflen){
	//KASSERT(curproc->filetable[fd]->file_vn != NULL);

	char path[] = "con:";
	int open_code = 0;
	if(curproc->filetable[fd]->file_vn == NULL){
		open_code = vfs_open(path, O_RDONLY, 0, &(curproc->filetable[fd]->file_vn));
	}
	if(open_code){
		return open_code;
	}
	(void) open_code;


	// check wheter the fd is Invalid
	//	kprintf("check_1\n");

	// if(fd < 0 || fd > 2){
	// 	kprintf("our of range\n");
	// 	return EBADF;
	// }
	//
	// if(curproc->filetable[fd]->flag != O_RDWR && curproc->filetable[fd]->flag != O_WRONLY){
	// 	kprintf("wrong flag! no %d\n",(int)curproc->filetable[fd]->flag);
	// 	return EBADF;
	// }

		//kprintf("check_2\n");
	//check the address of buf pointer
	int adr_check;
  	char *bufferName = kmalloc(buflen);
	adr_check = copyin((const_userptr_t)buf,bufferName,strlen(bufferName));




//	kprintf("adr_check %d\n",adr_check);
	if(adr_check){
		return EFAULT;
	}
	//kprintf("%s\n",(char *)buf);
	//bufferName[buflen+1] = '\0';

	/*
	 * Initialize a uio suitable for I/O from a kernel buffer.
	 *
	 * Usage example;
	 * 	char buf[128];
	 * 	struct iovec iov;
	 * 	struct uio myuio;
	 *
	 * 	uio_kinit(&iov, &myuio, buf, sizeof(buf), 0, UIO_READ);
	 *      result = VOP_READ(vn, &myuio);
	 *      ...
	 */
	// void uio_kinit(struct iovec *, struct uio *,
	// 	       void *kbuf, size_t len, off_t pos, enum uio_rw rw);

	struct iovec iov;
	struct uio myuio;
	uio_kinit(&iov, &myuio, bufferName, strlen(bufferName), curproc->filetable[fd]->offset, UIO_WRITE);
	// int (*vop_write)(struct vnode *file, struct uio *uio);
	adr_check = VOP_WRITE(curproc->filetable[fd]->file_vn, &myuio);


	return adr_check;
}
