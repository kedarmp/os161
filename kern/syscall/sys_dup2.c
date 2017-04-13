#include <types.h>
#include <syscall.h>
#include <fhandle.h>
#include <proc.h>
#include <current.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <kern/limits.h>
#include <file_close.h>
#include <sys_dup2.h>

int sys_dup2(int oldfd, int newfd, int *errptr) {
	//use lock
	//need to use global lock lock(notn existing one)
//kprintf("dup2: oldfd:%d, newfd:%d\n",oldfd,newfd);
int err;

//oldfd is not a valid file descriptor, or newfd is a value that cannot be a valid file descriptor.
if(oldfd>=__OPEN_MAX || oldfd<0 || newfd>=__OPEN_MAX || newfd<0) 
{
	*errptr = EBADF;
	return -1;
}

//The two handles refer to the same "open" of the file -- that is, they are references to the same object and share the same seek pointer.
if(oldfd == newfd) 
{
	*errptr = 0;
	return newfd;
}

struct fhandle *f_handle_name = (curproc->ftable[oldfd]);	
if(f_handle_name == NULL)
{
	*errptr = EBADF;
	return -1;
}
lock_acquire(f_handle_name->lock);
//kprintf("DUP2 fhandle:%s \n",f_handle_name->lock->lk_name);
//The process's file table was full, or a process-specific limit on open files was reached.
int i = 0;
int count = 0;
for(i = 0;i<__OPEN_MAX;i++)
{
	if(curproc->ftable[i] != NULL)
	{
		count++;
	}
}
if(count == __OPEN_MAX-1)
{
	*errptr = EMFILE;
	lock_release(f_handle_name->lock);
	return -1;	
}
lock_release(f_handle_name->lock);

// If newfd names an already-open file, that file is closed.
if(curproc->ftable[newfd]!=NULL) 
{
	err = sys_close(newfd, errptr);
	if(err) 
	{
		return -1; //errptr will have been set by sysclose
	}
}

lock_acquire(f_handle_name->lock);
//copy fhandle at oldfd into newfd
curproc->ftable[oldfd]->rcount++;
curproc->ftable[newfd] = curproc-> ftable[oldfd];
//Incerement the reader count.
//No errror

lock_release(f_handle_name->lock);
*errptr=0;
return newfd;
}


