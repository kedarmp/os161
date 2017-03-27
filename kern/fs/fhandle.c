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
	struct proc * parent = get_proc(curproc->parent_proc_id);
	struct fhandle * han = parent->ftable[fd];
	kprintf("doihold: %d\n",lock_do_i_hold(han->lock));
	lock_acquire(h->lock);
	if(h->rcount>=1)
	{
		h->rcount--;
	}
	if(h->rcount == 0) {
		vfs_close(h->file);
		lock_release(h->lock);
		lock_destroy(h->lock);
		kfree(h);
		h = NULL;
		kprintf("destroyed:%d\n",fd);
		curproc->ftable[fd] = NULL;
	}
	else
	{
		lock_release(h->lock);
	}
}

