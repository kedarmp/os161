#include <fhandle.h>
#include <file_read.h>
#include <uio.h>
#include <vnode.h>
#include <current.h>
#include <proc.h>
#include <kern/errno.h>
 

ssize_t sys_read(uint32_t fd_u, userptr_t buffer_u, uint32_t size_u) {
	
	//TO-DO all kinds of possible checks on arguments here
	
	if(buffer_u==NULL)
		return EINVAL;		
	if(fd_u >= __OPEN_MAX)	
		return EBADF;

	struct fhandle *f_handle_name = (curproc->ftable[fd_u]);	
	if(f_handle_name == NULL)
		return EBADF;

	lock_acquire(f_handle_name->lock);
	
	struct uio u;
	struct iovec iov;
	enum uio_rw e = UIO_READ;
	uio_uinit(&iov,&u,buffer_u,size_u,f_handle_name->write_offset,e);	//make sure that the unit params are correct(esp the segflg,uiospace)
	
	struct vnode *vnode = f_handle_name->file;	
	
	//perform the read!
	int err = VOP_READ(vnode,&u);
	if(err!=0) {
		lock_release(f_handle_name->lock);
		return err;
	}
	//kprintf("\nerr::%d",err);
	int bytes_read = (size_u - (u.uio_resid));
	//kprintf("Apparently we wrote something! : %d \n",bytes_written);
	f_handle_name->read_offset += bytes_read;

	lock_release(f_handle_name->lock);
	return bytes_read;
}
