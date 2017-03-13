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

#define MAX 1024

int sys_open(userptr_t user_filename, int flags, mode_t mode, int *retval){
	struct vnode *vn;
	int index = 3 ; // 0,1,2 for reserve spot
	int open_code;

	bool empty = false;
	 // (void) retval;
	// (void) user_filename;
	// (void) flags;
	// (void) mode;

	// kprintf("@@@@@@@@@@@@@@@@@@@@@@@@@@  passed flags = %d  @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n", flags);
	// kprintf("@@@@@@@@@@@@@@@@@@@@@@@@@@  user_filename =  %s  @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n", (char*)user_filename);

	if(user_filename == NULL){
		// no such file, non arg
		empty = true;
		return  EFAULT;
	}

	char *filename;
 // 	char *filename = (char*) user_filename;		 	// char *filename = (char*) user_filename
 	struct file_handler *file;
	file = kmalloc(sizeof(struct file_handler));
	filename = (char *)kmalloc(sizeof(char)*PATH_MAX);
  copyin((const_userptr_t)user_filename,filename,sizeof(filename));
	// if(flags > 2){
	// 	empty = true;
	// 	return EINVAL;
	// }

	// check if file table is available for later file_descriptor pointer to file objector, save spot for open

	// for(int i = 0 ; i < FILE_SIZE; ++i){
	// 	if(curproc->filetable[i] == NULL){
	// 		kprintf("@@@@@@@@@@@@@@@@@@@@@@@ free index =  %d  @@@@@@@@@@@@@@@@@@@@@@@\n", i);
	// 	}
	// }




	while(!empty || (index >= 3 && index < FILE_SIZE)){
		if(curproc->filetable[index] == NULL){
			empty = true;
			// kprintf("@@@@@@@@@@@@@@@@@@@@@@@ curproc->filetable[index] has space, index =  %d  @@@@@@@@@@@@@@@@@@@@@@@\n", index);
			// as long as it is empty, break;
			break;
		}
		index++;
	}

	if (!empty && index == FILE_SIZE){
		// file table is full
		return EMFILE;
	}
//	kprintf("@@@@@@@@@@@@@@@@@@@@@@@ double check index =  %d  @@@@@@@@@@@@@@@@@@@@@@@\n", index);



	open_code = vfs_open(filename,flags,mode,&vn);

//	kprintf("@@@@@@@@@@@@@@@@@@@@@@@ open_code =  %d  @@@@@@@@@@@@@@@@@@@@@@@\n", open_code);

	if(open_code){
		return open_code;
	}
	file->offset = 0;
 	file->file_vn = vn;
 	file->flag = flags;
	curproc->filetable[index] = file;
	*retval = index;
	// if(index > FILE_SIZE)
	// {
	// 	return -1;
	// }
	return 0;
}


int sys_read(int fd, void *buf,size_t buflen,int *retval){
	(void) fd;
	(void) buf;
	(void) buflen;

	if(fd < 0 || curproc->filetable[fd] == NULL){
		*retval = -1;
		return EBADF;
	}

	if(buf == NULL || buflen == 0){
		*retval = -1;
		return EFAULT;
	}

	if(curproc->filetable[fd]->flag == O_WRONLY){
		*retval = -1;
		return EBADF;
	}

	char path[] = "con:";
	int open_code = 0;
	if(curproc->filetable[fd]->file_vn == NULL){
		open_code = vfs_open(path, O_RDONLY, 0, &(curproc->filetable[fd]->file_vn));
		if(open_code){
			*retval = -1;
			return open_code;
		}
	}
	(void) open_code;

	int adr_check;
 //  	char *bufferName = kmalloc(buflen);
 	void *bufferName;
	bufferName = kmalloc(sizeof(*buf)*buflen);
	adr_check = copyin((const_userptr_t)buf,bufferName,buflen);

	if(adr_check){
	  *retval = -1;
		return EFAULT;
	}

	struct iovec iov;
	struct uio myuio;
	// uio_kinit(&iov, &myuio, bufferName, strlen(bufferName), curproc->filetable[fd]->offset, UIO_WRITE);
	// uio_kinit(&iov, &myuio, bufferName, strlen(bufferName), 0, UIO_WRITE);
	// uio_kinit(&iov, &myuio, bufferName, buflen, 0, UIO_READ);
	uio_kinit(&iov, &myuio, bufferName, buflen, curproc->filetable[fd]->offset, UIO_READ);

	//update the offset
	curproc->filetable[fd]->offset = buflen + curproc->filetable[fd]->offset;

	// int (*vop_write)(struct vnode *file, struct uio *uio);
	adr_check = VOP_READ(curproc->filetable[fd]->file_vn, &myuio);
	if(adr_check){
		*retval = -1;
		return adr_check;
	}

	adr_check = copyout(bufferName,buf,buflen);
	if(adr_check){
		*retval = -1;
		return adr_check;
	}

	//update offset in filetable
	curproc->filetable[fd]->offset = myuio.uio_offset;
  *retval = buflen - myuio.uio_resid;
	return 0;
}

int sys_write(int fd, const void *buf, size_t buflen, int *retval){
	//KASSERT(curproc->filetable[fd]->file_vn != NULL);

	if(fd < 0 || fd > FILE_SIZE || curproc->filetable[fd] == NULL){
		return EBADF;
	}
// this one should not be a error
	// if(buf == NULL || buflen == 0){
	// 	return -1;
	// }
// buf to 0 is totally ok
	if(curproc->filetable[fd]->flag == O_RDONLY){
		return EBADF;
	}

	char path[] = "con:";
	int open_code = 0;
	if(curproc->filetable[fd]->file_vn == NULL){
		open_code = vfs_open(path, O_RDONLY, 0, &(curproc->filetable[fd]->file_vn));
	}
	if(open_code){
		return open_code;
	}
	(void) open_code;

	int adr_check;
 //  	char *bufferName = kmalloc(buflen);
 	void *bufferName;
	bufferName = kmalloc(sizeof(*buf)*buflen);
	adr_check = copyin((const_userptr_t)buf,bufferName,buflen);

	if(adr_check){
		return EFAULT;
	}

	struct iovec iov;
	struct uio myuio;
	// uio_kinit(&iov, &myuio, bufferName, strlen(bufferName), curproc->filetable[fd]->offset, UIO_WRITE);
	// uio_kinit(&iov, &myuio, bufferName, strlen(bufferName), 0, UIO_WRITE);
	uio_kinit(&iov, &myuio, bufferName, buflen, curproc->filetable[fd]->offset, UIO_WRITE);
	// int (*vop_write)(struct vnode *file, struct uio *uio);
	adr_check = VOP_WRITE(curproc->filetable[fd]->file_vn, &myuio);

	if(adr_check){
		return adr_check;
	}
	curproc->filetable[fd]->offset = myuio.uio_offset;
	*retval = buflen;
 	return 0;
}

int sys_close(int fd){
	if(fd < 0 || fd > FILE_SIZE  || curproc->filetable[fd] == NULL){
		return EBADF;
	}
	if(curproc->filetable[fd]->file_vn != NULL){
		vfs_close(curproc->filetable[fd]->file_vn);
		kfree(curproc->filetable[fd]);
		curproc->filetable[fd] = NULL;
	}
	return 0;
}

int sys_lseek(int fd, off_t pos, int whence,int *retval, int *retval_1){

	struct stat statbuf;
	int response;
	(void) response;

	if (fd < 0 || fd > FILE_SIZE) {
        return EBADF;
    }

    if (curproc->filetable[fd] == NULL) {

        return EBADF;
    }

    uint32_t new_position;
    //lock_acquire(curproc->filetable[fd]->file_lk);
    switch(whence) {

        /* the new position is pos */
        case SEEK_SET:

            if (pos < 0){
                return EINVAL;
            }

            curproc->filetable[fd]->offset = pos;
            response = VOP_ISSEEKABLE(curproc->filetable[fd]->file_vn);
            if (!response) {
                return response;
            }
						new_position = curproc->filetable[fd]->offset;
            break;

            /* the new position is the current position plus pos */
        case SEEK_CUR:
            curproc->filetable[fd]->offset += pos;
						response = VOP_ISSEEKABLE(curproc->filetable[fd]->file_vn);
            if (!response) {
                return response;
            }
						new_position = curproc->filetable[fd]->offset;
            break;

            /* the new position is the position of end-of-file plus pos */
        case SEEK_END:
            response = VOP_STAT(curproc->filetable[fd]->file_vn, &statbuf);
            if (response) {

                return response;
            }
            curproc->filetable[fd]->offset = pos + statbuf.st_size;
						response = 0;
						response = VOP_ISSEEKABLE(curproc->filetable[fd]->file_vn);
            if (!response) {
                return response;
            }
            new_position = curproc->filetable[fd]->offset;
						//*retval = curproc->filetable[fd]->offset;
            break;

        default:
            return -1;
    }
		*retval = (new_position & 0xFFFFFFFF00000000) >> 32;
    *retval_1 = new_position & 0x00000000FFFFFFFF;
		// put 64 bit pos data into two 32 bit containe
		//lock_release(curproc->filetable[fd]->file_lk);
    return 0;
}
