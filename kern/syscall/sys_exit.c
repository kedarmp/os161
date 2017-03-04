#include <sys_exit.h>
#include <kern/wait.h>
#include <current.h>
#include <proc_table.h>

void sys_exit(int exitcode) {
	curproc->exit_code = _MKWAIT_EXIT(exitcode);
	struct proc * parent = get_proc(curproc->parent_proc_id);
	if(parent != NULL && parent->proc_id!=1)
		V(parent->sem); 	//sem used to synch with waitpid()
	thread_exit();
//	kprintf("\nShould not be called\n");
}
