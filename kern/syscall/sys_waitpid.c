#include <types.h>
#include <kern/errno.h>
#include <current.h>
#include <copyinout.h>
#include <sys_exit.h>
#include <kern/wait.h>
#include <proc_table.h>
#include <sys_waitpid.h>
#include <proc_table.h>

pid_t
sys_waitpid(pid_t pid, userptr_t status, int options,int *errptr) {
	if(options !=0)
	{
		panic("otions !=0");
		*errptr = EINVAL;
		return -1;
	}
	if(pid<2 || pid >= MAX_PROC) {
		panic("invalid pid!=0");
		*errptr = ESRCH;
		return -1;
	}
	struct proc *child = get_proc(pid);
	if(child == NULL)
	{
		panic("null get_proc");
		//kprintf("NULL_get_proc:%d\n",pid);
		*errptr = ESRCH;
		return -1;	
	}


	//make an exception for when curproc is [kernel] thread. Allows us to cleanup the first user process launched from menu.c, because cmd_common_prog stuff from menu.c
//does not make the kernel process the parent of the first user process(whose pid=2). In that sense,
//the first user process(e.g. 's'[hell] or 'p' are the real 'init' processes!)
//Maybe change the model later. Right now we dont care
	if(curproc->proc_id != child->parent_proc_id)
	{
		//kprintf("WRONG_PARENT:%d\n",pid);
		panic("mismatch parent:%d of child:%d",curproc->proc_id,child->proc_id);
		*errptr = ECHILD;
		return -1;
	}
	// kprintf("WAITPID The pid: %d , The curprocs pid: %d , The childs parents id: %d \n",pid,curproc->proc_id,child->parent_proc_id);


	lock_acquire(curproc->proc_lock);
	P(child->sem);
	//child called _exit
	if(status != NULL) {    //collect exitcode. //See manpage for status!=null 
        	if(WIFEXITED(child->exit_code) || WIFSIGNALED(child->exit_code))
	        {
	                //NOTE: The WEXITSTATUS,WTERMSIG etc macros are supposed to be called by the user. So, there is no  need to decode the exitcode back in the kernel space itself
	                int err_code = (child->exit_code);
	          
	                int err = copyout(&err_code, status, sizeof(int));
	                if(err) {
	                		//panic("Erro copying out wiatpi status\n");
	                        //*errptr = EFAULT;
	                        //even if copying out failed(e.g. because of bad ptr, do we still have to cleanup the child)? 7792
							proc_destroy(child);
							recycle_pid(pid);//in any case, recycle the pid of the child, since its exited
							lock_release(curproc->proc_lock);
							*errptr = 0;
	                        return pid;
	                        }
	         }
	}      
	*errptr = 0;
	if(child->p_numthreads == 0) {
		proc_destroy(child);
	        recycle_pid(pid);
	}
	// else {
	//We should NOT have pnumthreads=1 when after child exits. else we may have to busy wait?
//cleanup wont be performed. thats the only drawback
		//kprintf("Will leak.Child V'd.Child pnumthreads:%d\n",child->p_numthreads);
	// }
	lock_release(curproc->proc_lock);
	return pid;
}
