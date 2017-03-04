#include <types.h>
#include <kern/errno.h>
#include <current.h>
#include <copyinout.h>
#include <sys_exit.h>
#include <kern/wait.h>
#include <proc_table.h>
#include <sys_waitpid.h>

pid_t
sys_waitpid(pid_t pid, userptr_t status, int options,int *errptr) {
	

	if(pid < 2 || options !=0)
	{
		*errptr = EINVAL;
		return -1;
	}

	struct proc *child = get_proc(pid);
	if(child == NULL)
	{
		*errptr = ESRCH;
		return -1;	
	}


	if(curproc->proc_id != child->parent_proc_id)
	{
		*errptr = ECHILD;
		return -1;
	}

	//P(curproc->sem);
	
	//child called _exit
	if(WIFEXITED(child->exit_code)) 
	{	
		//try to copy out exit code to userptr
		int err_code = WEXITSTATUS(child->exit_code);
		kprintf("waitpid error code:%d\n",err_code);
		int err = copyout(&err_code, status, sizeof(int));
		if(err) {
			*errptr = EFAULT;
			return -1;
		}
		*errptr = 0;
		return pid;
	}
	*errptr = 0;
	return pid;
}