#include <fhandle.h>
#include <lib.h>
#include <vfs.h>

struct fhandle* fhandle_create(char *file_name, int open_mode) {
	struct fhandle *f;
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
		kprintf("\nError vfs_opening file. Errorcode:%d",err);
		lock_destroy(f->lock);
		kfree(f);
		return NULL;
	}

	return f;
}

void fhandle_destroy(struct fhandle *h) {
	lock_acquire(h->lock);
	h->rcount--;
	lock_release(h->lock);
	if(h->rcount == 0) {
		lock_destroy(h->lock);
		kfree(h);
	}
	

}

