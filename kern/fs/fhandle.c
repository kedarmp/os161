#include <fhandle.h>
#include <lib.h>
#include <vfs.h>

struct fhandle* fhandle_create(char *file_name, int open_mode) {
	struct fhandle *f;
	f = kmalloc(sizeof(*f));
	f->read_offset = 0;
	f->write_offset = 0;
	f->rcount = 0;

	f->lock = lock_create(file_name);
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

