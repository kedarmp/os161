#include <fhandle.h>
#include <lib.h>
#include <vfs.h>
#include <current.h>
#include <proc.h>

struct fhandle* fhandle_create(char *file_name, int open_mode,int *errptr) {
	struct fhandle *f;
	*errptr = 0;	//by default. Does not always mean success.Callers should check this value only on return NULL
	f = kmalloc(sizeof(*f));
	if(f==NULL)
		return NULL;
	f->offset = 0;
	f->rcount = 0;

	f->lock = lock_create(file_name);
	if(f->lock==NULL) {
		kfree(f);
		return NULL;
	}
	//initialize vnode
	int err = vfs_open(file_name,open_mode,0,&f->file);
	if(err) {
		*errptr = err;
		lock_destroy(f->lock);
		kfree(f);
		return NULL;
	}
	return f;
}

void fhandle_destroy(struct fhandle *h,int fd) {
	lock_acquire(h->lock);
	h->rcount--;
	lock_release(h->lock);

	if(h->rcount == 0) {
		if(fd==0 || fd==1 || fd==2)
		return;
		vfs_close(h->file);
		 lock_destroy(h->lock);
		 kfree(h);
		h = NULL;
		curproc->ftable[fd] = NULL;
	}
	
}

