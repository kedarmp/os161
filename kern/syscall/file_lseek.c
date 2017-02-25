#include <file_lseek.h>
#include <kern/errno.h>
#include <fhandle.h>
#include <kern/seek.h>
#include <kern/stat.h>
#include <limits.h>
#include <current.h>
#include <vnode.h>
#include <proc.h>

off_t sys_lseek(int fd, uint32_t high, uint32_t low, int *whence, int *errptr) {

	//File handle check
	if(fd >= __OPEN_MAX || fd < 0)	
	{
		*errptr = EBADF;
		return -1;
	}

	//Obtaining file handler
	struct fhandle *f_handle_name = (curproc->ftable[fd]);	
	if(f_handle_name == NULL)
	{
		*errptr = EBADF;
		return -1;
	}
	
	lock_acquire(f_handle_name->lock);
	//File handle console check
	if(fd==0 || fd==1 || fd==2 || !VOP_ISSEEKABLE(f_handle_name->file))	//!VOP_ISSEEKABLE(f_handle_name->file)
	{
		*errptr = ESPIPE;
		return -1;	
	}

	//taking two 32 bit and obtaining the 64 bit offset
	off_t off_set = (off_t)high;
	off_set = off_set << 32;
	off_set = off_set|low;

	//kprintf("OFFSET : %lld \n",off_set);
	if(off_set < 0)
	{	
		*errptr = EINVAL;
		lock_release(f_handle_name->lock);
		return -1;
	}

	switch(*whence)
	{
		case SEEK_SET:
		{
			f_handle_name -> offset = off_set;
		}
		break;
		case SEEK_CUR:
		{
			f_handle_name -> offset += off_set;
		}
		break;
		case SEEK_END:
		{
			struct stat file_info;
			int err = VOP_STAT(f_handle_name->file, &file_info);
			if(err)
			{
				*errptr = err;
				lock_release(f_handle_name->lock);
				return -1;
			}
			f_handle_name->offset = file_info.st_size + off_set;

		}
		break;
		default:
		{
			if(off_set < 0)
			{
				*errptr = EINVAL;
				lock_release(f_handle_name->lock);
				return -1;
			}
		}
		break;
	}
	*errptr = 0;
	lock_release(f_handle_name->lock);
	return (f_handle_name->offset);
}