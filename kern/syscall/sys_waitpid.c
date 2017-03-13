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

	if(options !=0)
	{
		*errptr = EINVAL;
		return -1;
	}
	if(pid<2 || pid >= MAX_PROC) {
		*errptr = ESRCH;
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

	
	P(curproc->sem);
	//child called _exit
	if(status != NULL) {    //collect exitcode. //See manpage for status!=null 
        	if(WIFEXITED(child->exit_code) || WIFSIGNALED(child->exit_code))
	        {
	                //NOTE: The WEXITSTATUS,WTERMSIG etc macros are supposed to be called by the user. So, there is no  need to decode the exitcode back in the kernel space itself
	                int err_code = (child->exit_code);
	              kprintf("err_code:%d\n",err_code);
	                int err = copyout(&err_code, status, sizeof(int));
	                if(err) {
	                        *errptr = EFAULT;
	                        recycle_pid(pid);//in any case, recycle the pid of the child, since its exited
	                        return -1;
	                        }
	         }
	}
   
	if(child->p_numthreads == 0) {
                recycle_pid(pid);
                // proc_destroy(child);
                }
        
	*errptr = 0;
	return pid;
}
