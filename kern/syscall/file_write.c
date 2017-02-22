#include <fhandle.h>
#include <file_write.h>
#include <uio.h>
#include <vnode.h>
#include <current.h>
#include <proc.h>
 
ssize_t sys_write(uint32_t fd_u, userptr_t buffer_u, uint32_t size_u) {
	
	//TO-DO all kinds of possible checks on arguments here
	
	KASSERT(buffer_u!=NULL);

	struct fhandle *f_handle_name = (curproc->ftable[fd_u]);	
	KASSERT(f_handle_name != NULL);

	lock_acquire(f_handle_name->lock);
	//kprintf("Filehandle:%d, sizze:%d\n:",fd_u,size_u);
	
	struct uio u;
	struct iovec iov;
	enum uio_rw e = UIO_WRITE;
	uio_uinit(&iov,&u,buffer_u,size_u,f_handle_name->write_offset,e);
	
	struct vnode *vnode = f_handle_name->file;	
	
	//perform the write!
	//int err=1;
	int err = VOP_WRITE(vnode,&u);
	if(err!=0) {
		lock_release(f_handle_name->lock);
		return err;
	}
	//kprintf("\nerr::%d",err);
	int bytes_written = (size_u - (u.uio_resid));
	//kprintf("Apparently we wrote something! : %d \n",bytes_written);
	f_handle_name->write_offset += bytes_written;

	lock_release(f_handle_name->lock);
	return bytes_written;
}
