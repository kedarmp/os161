#include <fhandle.h>
#include <file_write.h>
#include <uio.h>
#include <vnode.h>
#include <current.h>
#include <proc.h>
#include <kern/errno.h>
 
ssize_t sys_write(uint32_t fd_u, userptr_t buffer_u, uint32_t size_u,int *errptr) {
	
	//TO-DO all kinds of possible checks on arguments here
	if(buffer_u==NULL)
	{
		*errptr = EINVAL;
		return -1;
	}		
	if(fd_u >= __OPEN_MAX)	
	{
		*errptr = EBADF;
		return -1;
	}

	struct fhandle *f_handle_name = (curproc->ftable[fd_u]);	
	if(f_handle_name == NULL)
	{
		*errptr = EBADF;
		return -1;
	}

	lock_acquire(f_handle_name->lock);
	//kprintf("Filehandle:%d, sizze:%d\n:",fd_u,size_u);
	
	struct uio u;
	struct iovec iov;
	enum uio_rw e = UIO_WRITE;
	uio_uinit(&iov,&u,buffer_u,size_u,f_handle_name->offset,e);
	
	struct vnode *vnode = f_handle_name->file;	
	
	//perform the write!
	//int err=1;
	int err = VOP_WRITE(vnode,&u);
	if(err!=0) {
		lock_release(f_handle_name->lock);
		*errptr = err;
		return -1;
	}
	//kprintf("\nerr::%d",err);
	int bytes_written = (size_u - (u.uio_resid));
	//kprintf("Apparently we wrote something! : %d \n",bytes_written);
	f_handle_name->offset += bytes_written;

	lock_release(f_handle_name->lock);
	//Success throughout , therefore reset the errptr
	*errptr = 0;
	return bytes_written;
}
