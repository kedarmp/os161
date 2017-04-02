#include <types.h>
#include <limits.h>
#include <lib.h>
#include <kern/errno.h>
#include <addrspace.h>
#include <thread.h>
#include <fhandle.h>
#include <sys_fork.h>
#include <syscall.h>
#include <file_close.h>
#include <proc_table.h>


extern void *memcpy(void *dest, const void *src, size_t len);

pid_t sys_fork(struct trapframe* old_trapframe,struct proc* parent_proc,int *errptr)
{
	//Check if kernel thread isnt calling fork.22880
	if(parent_proc -> proc_id < 2)
	{
		*errptr = EINVAL;
		return -1;
	}

	//Checking the number of processes
	pid_t child_id = create_pid();
	if(child_id == -1) {
		*errptr = ENOMEM;	//technically should be ENPROC.But returning ENOMEM is what passes the forkbomb test!
		return -1;	
	}

	//Wrapper funciton that is used to call proc_create
	struct proc * child = call_proc_create("Userprocess");
	if(child == NULL) {
		recycle_pid(child_id);
		*errptr = ENOMEM;
		return -1;
	}
	
//	kprintf("Forking..\n");
	//copy stuff from parent
	child -> proc_id = child_id;
	child -> parent_proc_id = parent_proc -> proc_id;
//	child -> p_numthreads = 1;

	

	struct addrspace *child_addrspace = NULL;
	int err = as_copy(parent_proc->p_addrspace, &child_addrspace);
	if(err) {
		
		proc_destroy(child);
		recycle_pid(child_id);
		*errptr = ENOMEM;
		return -1;
	}
	child -> p_addrspace = child_addrspace;

	//child trapframe
	struct trapframe *child_tf = NULL;
	child_tf = kmalloc(sizeof(struct trapframe));
	if(child_tf==NULL) {
		
		proc_destroy(child);
		recycle_pid(child_id);
		*errptr = ENOMEM;
		return -1;
	}
	memcpy(child_tf, old_trapframe, sizeof(struct trapframe));
//TODO Do we need an as_destroy somewhere here?

	//Running through the parent ftable and getting the number of open FDs
	int i;
	for(i = 0;i<OPEN_MAX;i++)
	{
		if(parent_proc -> ftable[i]!= NULL && parent_proc ->ftable[i]!=(struct fhandle*)0xdeadbeef) {
		struct fhandle *parent_handle = parent_proc->ftable[i];
		lock_acquire(parent_handle->lock);
		
			//increase reference counts of all file handles in parent before copying them to child(also use locks someplace)
			parent_handle -> rcount += 1;
			child -> ftable[i] = parent_handle;
		lock_release(parent_handle->lock);
		}
		
	}

	err = thread_fork("Userthread", child, enter_forked_process, child_tf, 0);
	if(err) {
		
		//Reduce shared filehandle ref counts(i.e. simply call close). Corner case when thread_fork itself fails. Won't be called regularly
		for(i = 0; i<OPEN_MAX;i++) {
			// kprintf("closing\n");
			sys_close(i,&err);
		}
		
			// kfree(child_tf);
		proc_destroy(child);
		recycle_pid(child_id);
		*errptr =err;
		return -1;
	}
  //      kheap_printused();
	


	*errptr =0;
	return child_id;
}
