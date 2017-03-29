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
	kprintf("%d waitpiding on %d\n",curproc->proc_id,pid);
	P(curproc->sem);
	kprintf("waitpid-child exited\n");
//	while(child->p_numthreads!=0);//keep waiting
	//child called _exit
	if(status != NULL) {    //collect exitcode. //See manpage for status!=null 
        	if(WIFEXITED(child->exit_code) || WIFSIGNALED(child->exit_code))
	        {
	                //NOTE: The WEXITSTATUS,WTERMSIG etc macros are supposed to be called by the user. So, there is no  need to decode the exitcode back in the kernel space itself
	                int err_code = (child->exit_code);
	          
	                int err = copyout(&err_code, status, sizeof(int));
	                if(err) {
	                        *errptr = EFAULT;
	                        recycle_pid(pid);//in any case, recycle the pid of the child, since its exited
	                        //even if copying out failed(e.g. because of bad ptr, do we still have to cleanup the child)? 7792
	                        if(child->p_numthreads == 0) {
								proc_destroy(child);
							}
	                        return -1;
	                        }
	         }
	}      
	*errptr = 0;
	if(child->p_numthreads == 0) {
		proc_destroy(child);
	        recycle_pid(pid);
	}
	else {
	//We should NOT have pnumthreads=1 when after child exits. else we may have to busy wait?
//cleanup wont be performed. thats the only drawback
		kprintf("Will leak.Child V'd.Child pnumthreads:%d\n",child->p_numthreads);
	}
	return pid;
}
