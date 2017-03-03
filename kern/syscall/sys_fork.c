#include <types.h>
#include <limits.h>
#include <lib.h>
#include <kern/errno.h>
#include <addrspace.h>
#include <thread.h>
#include <fhandle.h>
#include <sys_fork.h>
#include <syscall.h>

extern void *memcpy(void *dest, const void *src, size_t len);

pid_t sys_fork(struct trapframe* old_trapframe,struct proc* parent_proc,int *errptr)
{
	//Check if kernel thread isnt calling fork
	//if(parent_proc -> proc_id < 2)
	// {
	// 	*errptr = EINVAL;
	// 	return -1;
	// }

	//Checking the number of processes
	pid_t child_id = create_pid();
	// if(child_id == -1) {
	// 	*errptr = ENPROC;
	// 	return -1;	
	// }

	struct proc * child = call_proc_create("Userprocess");
	// if(child == NULL) {
	// 	return -1;
	// }
	
	//copy stuff from parent
	child -> proc_id = child_id;
	child -> parent_proc_id = parent_proc -> proc_id;
	child -> p_numthreads = 1;

	//increase reference counts of all file handles in parent before copying them to child(also use locks someplace)

	//Running through the parent ftable and getting the number of open FDs
	//int count_fd=0;
	int i;
	for(i = 0;i<OPEN_MAX;i++)
	{
		if(parent_proc -> ftable[i]!= NULL)
		{
			child -> ftable[i] = parent_proc -> ftable[i];
		}
	}

	//child->ftable = (struct fhandle*)kmalloc(sizeof(struct fhandle)*count_fd);
	
	//memcpy(child -> ftable, parent_proc -> ftable, (sizeof(struct fhandle*)*count_fd)); //sizeof((struct fhandle*)*OPEN_MAX));?
	struct addrspace *child_addrspace;
	as_copy(parent_proc->p_addrspace, &child_addrspace);
	child -> p_addrspace = child_addrspace;

	//child trapframe
	struct trapframe *child_tf = NULL;
	child_tf = kmalloc(sizeof(struct trapframe));
	memcpy(child_tf, old_trapframe, sizeof(struct trapframe));

	int err = thread_fork("Userthread", child, enter_forked_process, child_tf, 0);
	if(err) {
		*errptr =err;
		return -1;
	}
	*errptr =0;
	return child_id;

}