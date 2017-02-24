#include <fhandle.h>
#include <file_getcwd.h>
#include <uio.h>
#include <vfs.h>
#include <current.h>
#include <proc.h>
#include <limits.h>
#include <kern/errno.h>

ssize_t sys_getcwd(userptr_t buff_u, size_t bufflen, int * errptr) {	
	//TO-DO all kinds of possible checks on arguments here
	if(buff_u==NULL) {
		*errptr = EINVAL;
		return -1;
	}		
	if(bufflen < 1) {
		*errptr = EFAULT;
		return -1;
	}
	
	//example taken straight from menu.ci
	char dirname[PATH_MAX+1];
        struct iovec iov;
        struct uio u;
	enum uio_rw e = UIO_READ;
        uio_kinit(&iov, &u, dirname, PATH_MAX, 0, e);	//kinit because dirname is a kernel space buffer. 
	int err = vfs_getcwd(&u);
        if(err) {
                kprintf("couldn't get current working dir\n");
		*errptr = err;
		return -1;
        }

        dirname[PATH_MAX - u.uio_resid] = 0;
	//make sure that strlen(dirname)>=bufflen
	if(strlen(dirname) >= bufflen) {	//user provided buffer is small, return an error
		*errptr = ENAMETOOLONG;
		return -1;
	}	
	size_t got=0;	//we only need got
	err = copyoutstr(dirname,buff_u,PATH_MAX+1,&got);
	if(err) {
		kprintf("couldn't get current working dir\n");
		*errptr = err;
		return -1;
	}

	*errptr = 0;
	return got;	//return actual length of data returned
}
