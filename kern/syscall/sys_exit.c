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


	//close files for everybody regardless of PID
	int i, err;
	for(i=0;i<OPEN_MAX;i++){
		sys_close(i,&err);
	}	

	thread_exit();	//V is done in proc_remthread in proc.c which is called by thread_exit
	kprintf("\nShould not be called\n");
}
