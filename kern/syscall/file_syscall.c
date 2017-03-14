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

#define MAX 1024

int sys_open(userptr_t user_filename, int flags, mode_t mode, int *retval){
	struct vnode *vn;
	int index = 3 ; // 0,1,2 for reserve spot
	int open_code;
	bool empty = false;
	if(user_filename == NULL){
		// no such file, non arg
		empty = true;
		*retval = -1;
		return  EFAULT;
	}

	if(flags > 64 ||flags < 0){
		empty = true;
		*retval = -1;
		return EINVAL;
	}
	char *filename;
 // 	char *filename = (char*) user_filename;		 	// char *filename = (char*) user_filename
 	struct file_handler *file;
	file = kmalloc(sizeof(struct file_handler));
	filename = (char *)kmalloc(sizeof(char)*PATH_MAX);
  open_code = copyin((const_userptr_t)user_filename,filename,sizeof(filename));
	if(open_code){
		*retval = -1;
		return EFAULT;
	}
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
		*retval = -1;
		return EMFILE;
	}
//	kprintf("@@@@@@@@@@@@@@@@@@@@@@@ double check index =  %d  @@@@@@@@@@@@@@@@@@@@@@@\n", index);



	open_code = vfs_open(filename,flags,mode,&vn);

//	kprintf("@@@@@@@@@@@@@@@@@@@@@@@ open_code =  %d  @@@@@@@@@@@@@@@@@@@@@@@\n", open_code);

	if(open_code){
		*retval = -1;
		return open_code;
	}
	file->offset = 0;
 	file->file_vn = vn;
 	file->flag = flags;
	file->file_lk = lock_create("normal open file");
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

	if(fd < 0 || curproc->filetable[fd] == NULL || fd > FILE_SIZE){
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
  lock_acquire(curproc->filetable[fd]->file_lk);
	char path[] = "con:";
	int open_code = 0;
	if(curproc->filetable[fd]->file_vn == NULL){
		open_code = vfs_open(path, O_RDONLY, 0, &(curproc->filetable[fd]->file_vn));
		if(open_code){
			*retval = -1;
			lock_release(curproc->filetable[fd]->file_lk);
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
		lock_release(curproc->filetable[fd]->file_lk);
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
		lock_release(curproc->filetable[fd]->file_lk);
		return adr_check;
	}

	adr_check = copyout(bufferName,buf,buflen);
	if(adr_check){
		*retval = -1;
		lock_release(curproc->filetable[fd]->file_lk);
		return adr_check;
	}

	//update offset in filetable
	curproc->filetable[fd]->offset = myuio.uio_offset;
  *retval = buflen - myuio.uio_resid;
	lock_release(curproc->filetable[fd]->file_lk);
	return 0;
}

int sys_write(int fd, const void *buf, size_t buflen, int *retval){
	//KASSERT(curproc->filetable[fd]->file_vn != NULL);

	if(fd < 0 || fd > FILE_SIZE || curproc->filetable[fd] == NULL){
		*retval = -1;
		return EBADF;
	}
// this one should not be a error
	// if(buf == NULL || buflen == 0){
	// 	return -1;
	// }
// buf to 0 is totally ok
  if(buf == NULL){
		*retval = -1;
		return EFAULT;
	}
	if(curproc->filetable[fd]->flag == O_RDONLY){
			*retval = -1;
			return EBADF;
	}
 lock_acquire(curproc->filetable[fd]->file_lk);
	char path[] = "con:";
	int open_code = 0;
	if(curproc->filetable[fd]->file_vn == NULL){
		open_code = vfs_open(path, O_RDONLY, 0, &(curproc->filetable[fd]->file_vn));
	}
	if(open_code){
		*retval = -1;
		lock_release(curproc->filetable[fd]->file_lk);
		return open_code;
	}
	(void) open_code;

	int adr_check;
 //  	char *bufferName = kmalloc(buflen);
 	void *bufferName;
	bufferName = kmalloc(sizeof(*buf)*buflen);
	adr_check = copyin((const_userptr_t)buf,bufferName,buflen);
	if(adr_check){
		*retval = -1;
		lock_release(curproc->filetable[fd]->file_lk);
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
		lock_release(curproc->filetable[fd]->file_lk);
		*retval = -1;
		return adr_check;
	}
	curproc->filetable[fd]->offset = myuio.uio_offset;
	*retval = buflen;
	lock_release(curproc->filetable[fd]->file_lk);
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


int sys_lseek(int fd, off_t pos, int whence,int *retval, int *retval_1, uint64_t new_position){

	(void) retval;
	(void) retval_1;
	(void) new_position;
	struct stat statbuf;
	int response;
	(void) response;

	if (fd < 0 || fd > FILE_SIZE || curproc->filetable[fd] == NULL) {
		*retval = -1;
        return EBADF;
    }

	if (pos < 0){
		*retval = -1;
		return EINVAL;
	}

    lock_acquire(curproc->filetable[fd]->file_lk);

    //lock_acquire(curproc->filetable[fd]->file_lk);
    switch(whence) {

        case SEEK_SET:	// the new position is pos
            curproc->filetable[fd]->offset = pos;
            response = VOP_ISSEEKABLE(curproc->filetable[fd]->file_vn);
            if (!response) {
				*retval = -1;
                return response;
            }
			new_position = (uint64_t)(curproc->filetable[fd]->offset);
            break;

        case SEEK_CUR: // the new position is the current position plus pos
            curproc->filetable[fd]->offset += pos;
			response = VOP_ISSEEKABLE(curproc->filetable[fd]->file_vn);
            if (!response) {
				*retval = -1;
                return response;
            }
			new_position =(uint64_t)( curproc->filetable[fd]->offset);
            break;


        case SEEK_END: // the new position is the position of end-of-file plus pos
            response = VOP_STAT(curproc->filetable[fd]->file_vn, &statbuf);
            if (response) {
                *retval = -1;
                return response;
            }
            curproc->filetable[fd]->offset = pos + statbuf.st_size;
			response = 0;
			response = VOP_ISSEEKABLE(curproc->filetable[fd]->file_vn);
            if (!response) {
				*retval = -1;
                return response;
            }
            new_position =(uint64_t)( curproc->filetable[fd]->offset);
			//*retval = curproc->filetable[fd]->offset;
            break;

        default:
            return -1;
    }

		// split 64bits to 32bits
		split64to32(new_position, (uint32_t *)retval, (uint32_t*)retval_1);
		// if fail the split64to32(), do manually as following
 		/*         *retval = (new_position & 0xFFFFFFFF00000000) >> 32;
		/ *retval_1 = new_position & 0x00000000FFFFFFFF;     */

		lock_release(curproc->filetable[fd]->file_lk);
		// put 64 bit pos data into two 32 bit containe
		//lock_release(curproc->filetable[fd]->file_lk);
    return 0;
}


int sys_dup2(int old_fd, int new_fd,int *retval){
	  if (old_fd < 0 || old_fd > FILE_SIZE) {
			  *retval = -1;
        return EBADF;
    }
		if (new_fd < 0 || new_fd > FILE_SIZE) {
			  *retval = -1;
				return EBADF;
		}

    if (curproc->filetable[old_fd] == NULL) {
			  *retval = -1;
        return EBADF;
    }
   if(old_fd == new_fd){
		 *retval = new_fd;
		 return 0;
	 }
		if(curproc->filetable[new_fd] == NULL){
			lock_acquire(curproc->filetable[old_fd]->file_lk);
			curproc->filetable[new_fd] = kmalloc(sizeof(struct file_handler));
			curproc->filetable[new_fd]->file_vn = curproc->filetable[old_fd]->file_vn;
		 	curproc->filetable[new_fd]->flag = curproc->filetable[old_fd]->flag;
		 	curproc->filetable[new_fd]->offset = curproc->filetable[old_fd]->offset;
			curproc->filetable[new_fd]->file_lk = lock_create("cloned file");
			lock_release(curproc->filetable[old_fd]->file_lk);
		}else{
      int err;
			err = sys_close(old_fd);
			if(err){
				*retval = -1;
				return err;
			}
			lock_acquire(curproc->filetable[old_fd]->file_lk);
			curproc->filetable[new_fd] = kmalloc(sizeof(struct file_handler));
			curproc->filetable[new_fd]->file_vn = curproc->filetable[old_fd]->file_vn;
			curproc->filetable[new_fd]->flag = curproc->filetable[old_fd]->flag;
			curproc->filetable[new_fd]->offset = curproc->filetable[old_fd]->offset;
			curproc->filetable[new_fd]->file_lk = lock_create("cloned file");
			lock_release(curproc->filetable[old_fd]->file_lk);
		}
		*retval = new_fd;
		return 0;
}


int sys_chdir(userptr_t pathname,int *retval){
	if(pathname == NULL){
		// no such file, non arg
		*retval = -1;
		return  EFAULT;
	}
	char *path;
	int err;
	path = (char *)kmalloc(sizeof(char)*PATH_MAX);
 // 	char *filename = (char*) user_filename;		 	// char *filename = (char*) user_filename
	err = copyin((userptr_t)pathname,path,sizeof(path));
	if(err){
		*retval = -1;
		kfree(path);
		return err;
	}
	err = vfs_chdir(path);
	if(err){
		*retval = -1;
		kfree(path);
		return err;
	}
	*retval = 0;
	return 0;
}

int sys___getcwd(char *fname,size_t buflen, int *retval){
	if((int)buflen < 0){
		*retval = -1;
		return EFAULT;
	}
	int adr_check;
 //  	char *bufferName = kmalloc(buflen);
	void *bufferName;
	bufferName = kmalloc(sizeof(buflen));
	struct iovec iov;
	struct uio myuio;
	uio_kinit(&iov, &myuio, bufferName, buflen, 0, UIO_READ);
  // uio_kinit(&iov, &myuio, bufferName, buflen - 1 , 0, UIO_READ);
	//update the offset
	adr_check = vfs_getcwd(&myuio);
	if(adr_check){
		*retval = -1;
		return adr_check;
	}
	adr_check = copyout(bufferName,(userptr_t)fname,buflen);
	if(adr_check){
		*retval = -1;
		return adr_check;
	}
	*retval = buflen - myuio.uio_resid;
	return 0;
}
