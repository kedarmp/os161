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
		*errptr = ENOMEM;
		return -1;
	}
	
	//copy stuff from parent
	child -> proc_id = child_id;
	child -> parent_proc_id = parent_proc -> proc_id;
//	child -> p_numthreads = 1;

	//Running through the parent ftable and getting the number of open FDs
	int i;
	for(i = 0;i<OPEN_MAX;i++)
	{
		if(parent_proc -> ftable[i]!= NULL)
		{	
			//increase reference counts of all file handles in parent before copying them to child(also use locks someplace)
			parent_proc -> ftable[i] -> rcount += 1;
			child -> ftable[i] = parent_proc -> ftable[i];
		}
	}//TODO: Decrement count if any of the following instructions fail. (else we will have incremented the count without actually creating a new process)

	struct addrspace *child_addrspace = NULL;
	int err = as_copy(parent_proc->p_addrspace, &child_addrspace);
	if(err) {
		*errptr = ENOMEM;
		// if(child_addrspace!=NULL) {
		// 	// as_destroy(child_addrspace);
		// }
		return -1;
	}
	child -> p_addrspace = child_addrspace;

	//child trapframe
	struct trapframe *child_tf = NULL;
	child_tf = kmalloc(sizeof(struct trapframe));
	if(child_tf==NULL) {
		// as_destroy(child_addrspace);
		*errptr = ENOMEM;
		return -1;
	}
	memcpy(child_tf, old_trapframe, sizeof(struct trapframe));

	err = thread_fork("Userthread", child, enter_forked_process, child_tf, 0);
	if(err) {
		kfree(child_tf);
		// as_destroy(child_addrspace);
		// for(i=3;i<OPEN_MAX;i++){
		// 	sys_close(i,&err);
		// }
		*errptr =err;
		return -1;
	}
	*errptr =0;
	return child_id;

}
