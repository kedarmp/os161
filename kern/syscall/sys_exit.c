#include <sys_exit.h>
#include <kern/wait.h>
#include <current.h>
#include <proc_table.h>

void sys_exit(int exitcode) {
	kprintf("exit error code:%d\n",exitcode);
	curproc->exit_code = _MKWAIT_EXIT(exitcode);
	//struct proc * parent = get_proc(curproc->parent_proc_id);
	//sem used to synch with waitpid()
	//V(parent->sem);
	thread_exit();
//	kprintf("\nShould not be called\n");
}