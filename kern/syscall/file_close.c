#include <file_close.h>
#include <fhandle.h>
#include <vfs.h>
#include <current.h>
#include <proc.h>
#include <kern/errno.h>
#include <kern/stat.h>

int sys_close(int fd,int *errptr) {

	if(fd<0 || fd >= __OPEN_MAX)	
	{
		*errptr = EBADF;
		return -1;
	}

	struct fhandle *f_handle_name = (curproc->ftable[fd]);	
	if(f_handle_name == NULL  || f_handle_name == (struct fhandle *)0xdeadbeef)
	{
		*errptr = EBADF;
		return -1;
	}
	for(int d=4;d<6;d++)
                kprintf("other adresses:%p\n",curproc->ftable[d]);

	kprintf("sys_close(%d)\n",fd);
	fhandle_destroy(f_handle_name,fd);
	*errptr = 0;
	return 0;
}
