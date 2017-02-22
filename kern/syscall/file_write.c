
#include<file_write.h>
#include<uio.h>
//#include<iovec.h>
#include<vnode.h>
#include<current.h>
#include<proc.h>

ssize_t sys_write(uint32_t fd_u, userptr_t buffer_u, uint32_t size_u) {
	
//Do all kinds of possible checks on arguments here
	
	kprintf("Filehandle:%d, sizze:%d\n:",fd_u,size_u);
	
	struct uio *u;
	struct iovec *iov;
	enum uio_rw e = UIO_WRITE;



	char *buffer = kmalloc(sizeof(char)*size_u);
	size_t got;
	int err = copyinstr(buffer_u, buffer,size_u,&got);
	if(err) {
		kprintf("copyinstr err:%d",err);
		return err;
	}
	else {
		kprintf("%s",buffer);
		return got;
	}
	buffer[got] = '\0';	//use got or the advertized size_u?
	kprintf("\nGot %d bytes/chars. Had %d bytes/chars\n",got,size_u);

	
	uio_uinit(iov, u, buffer_u,size_u,0,e);
	
	//if fd_u ==1
	struct vnode *stdout_vnode = (curproc->ftable[1])->file;	
	//perform the write!
	err = VOP_WRITE(stdout_vnode,u);
	if(err) {
		kprintf("Writing err:%d",err);
		return err;
	}
	else {
		kprintf("Apparently we wrote something!");
		return got;
	}
	return 0;
}
