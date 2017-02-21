#include <types.h>
#include <copyinout.h>
#include <syscall.h>

int sys_open(userptr_t user_filename, int flags){
	(void) user_filename;
	(void) flags;
	return 0;
}
