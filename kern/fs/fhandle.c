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

//refactor this?
void fhandle_destroy(struct fhandle *h,int fd) {
/*	kprintf("Destroy fd:%d\n",fd);
	kprintf("h:%p",h);
	kprintf("do_i_hold: %d\n",lock_do_i_hold(h->lock));*/
	lock_acquire(h->lock);
//	kprintf("ACKed\n");
	curproc->ftable[fd] = NULL;
	h->rcount--;
	kprintf("fhandle-destroy fd:%d.Count:%d\n",fd,h->rcount);
	if(h->rcount == 0) {
		kprintf("destroying fd:%d\n",fd);
		vfs_close(h->file);
		lock_release(h->lock);
		lock_destroy(h->lock);
		kfree(h);
//		h = NULL;
		return;
	}
	lock_release(h->lock);
}

