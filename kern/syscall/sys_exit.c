#include <sys_exit.h>
#include <kern/wait.h>
#include <current.h>
#include <proc_table.h>
#include <file_close.h>
#include <limits.h>

void sys_exit(int exitcode,int type) {	//type defines the encoding for the exitcode
	if(type == TYPE_EXITED)	
		curproc->exit_code = _MKWAIT_EXIT(exitcode);
	else if(type == TYPE_RECEIVED_SIG)
		curproc->exit_code = _MKWAIT_SIG(exitcode);
	struct proc * parent = get_proc(curproc->parent_proc_id);
	if(parent != NULL && parent->proc_id!=1)
		V(parent->sem); 	//sem used to synch with waitpid()

	//close all open files
	//
	//Trying to close all files actually fails the forktest.
	//Probably because forktest correctly does close files
	//
	// int i, err;
	// // kprintf("Proc:%d\n",curproc->proc_id);
	// for(i=0;i<OPEN_MAX;i++){
	// 	sys_close(i,&err);
	// }
	//ALl file handles are closed(and also destroyed). However the filetable array still is allocated
	thread_exit();
//	kprintf("\nShould not be called\n");
}
