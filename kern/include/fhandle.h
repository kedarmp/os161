#ifndef _FHANDLE_H_
#define _FHANDLE_H_

#include <types.h>
#include <vnode.h>
#include <synch.h>


struct fhandle {
	struct vnode* file;
	off_t offset;
	int open_mode;	// File open modes. e.g. O_WRONLY (See fcntl.h)
	int rcount;	// Number of processes holding reference to this file handle
	struct lock *lock;
	struct lock *rwlock;
	
};

struct fhandle* fhandle_create(char *file_name, int open_mode,int *errptr);
void fhandle_destroy(struct fhandle *h,int fd);				//should dealloc only if rcount=0


#endif