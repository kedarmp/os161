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

/*	kprintf("Dumping proctable:\n");
	int j=0;
	for(j=0;j<3;j++) {
		struct proc * p = get_proc(j);
		if(p!=NULL) {
			kprintf("%d:%s\n",j,p->p_name);
		}
	}*/
	struct proc * parent=NULL;
	if(curproc->parent_proc_id !=0 ) {
	parent = get_proc(curproc->parent_proc_id);
	 kprintf("EXIT:%d.Numthreads:%d\n",curproc->proc_id,curproc->p_numthreads);
	}

	//close files for everybody regardless of PID
	int i, err;
	for(i=0;i<OPEN_MAX;i++){
		sys_close(i,&err);
	}	
	if(parent != NULL)
		V(parent->sem); 	//sem used to synch with waitpid()

	thread_exit();
	kprintf("\nShould not be called\n");
}
