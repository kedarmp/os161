#include <fhandle.h>
#include <file_open.h>
#include <uio.h>
#include <vnode.h>
#include <current.h>
#include <proc.h>
#include <kern/errno.h>
#include <kern/stat.h>
 
int sys_open(const_userptr_t filename, int flags,int *errptr) {	
	//TO-DO all kinds of possible checks on arguments here
	if(filename==NULL)
	{
		*errptr = EFAULT;
		return -1;
	}      
        

	char buffer[__PATH_MAX];
	size_t got;
	int err = copyinstr(filename, buffer, __PATH_MAX, &got);
	if(err!=0) 
	{
		*errptr = err;
		return -1;	//or EFAULT?
	}
	//kprintf("Opening file:%s",buffer);

	//check if there is space in our file table to open another file
	int count = 0;
	for(;count<__OPEN_MAX && curproc->ftable[count]!=NULL;count++)
	;	//linear search
	if(count<__OPEN_MAX) {
		//create a new file handle
		//see what happens if a process tries to reopen the same file. Does this fail? If it does, then we're good to go else we need manual checking of some sort
		struct fhandle *handle = fhandle_create(buffer, flags,&err);
		if(handle == NULL) {
			if(err) {	//vfs_open only returns 8 (EINVAL) if flags are incorrect
				*errptr = err;
				return -1; 
			}
		}
		curproc->ftable[count] = handle;
		handle->rcount++;
		handle->open_mode = flags;
		if(flags == (O_WRONLY|O_APPEND) || flags == (O_RDWR|O_APPEND)) {
			//get size of file
			struct stat file_info;
			err = VOP_STAT(handle->file, &file_info);
			if(err)
			{
				*errptr = err;
				return -1;
			}
			handle->offset = file_info.st_size;	//verify if st_size should be the offset
		}

	} 
	else 
	{	//too many files open
		*errptr = EMFILE;
		return -1;
	}
	//Success throughout , therefore reset the errptr
	*errptr = 0;
	return count;
}
