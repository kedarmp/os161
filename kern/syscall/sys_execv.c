#include <sys_execv.h>
#include <thread.h>
#include <limits.h>
#include <copyinout.h>

int sys_execv(char* user_progname, char** user_args, int *errptr) {
	int i = 0, j = 0, n_args = 0, err = 0;
	char **args;
	char progname[PATH_MAX];
	size_t got;
	while(user_args[n_args++]!=NULL)
		;
	args = kmalloc(sizeof(char *)*(--n_args));	//n_args strings
	i=0;
	//extract each string argument via copyinstr
	while(user_args[i]!=NULL) {
		while(user_args[i][j++]!='\0')
			;
		args[i] = kmalloc(sizeof(char)*j);
		err = copyinstr((const_userptr_t)user_args[i],args[i],j,&got);
		if(err) {
			*errptr = err;
			return -1;
		}
		i++;
	}
	//checking
	for(i=0;i<n_args;i++)
		kprintf("Argument %d:%s\n",i+1,args[i]);

	err = copyinstr((const_userptr_t)user_progname, progname, PATH_MAX, &got);
	if(err!=0) 
	{
		*errptr = err;
		return -1;	//or EFAULT?
	}
	kprintf("Programe name:%s\n",progname);	
	//at this point, 'n_args' arguments have been copied correctly(via copyin*) into args[]


	//Old runprogram code starts here
		
	// struct addrspace *as;
	// struct vnode *v;
	// vaddr_t entrypoint, stackptr;
	// int result;

	// /* Open the file. */
	// result = vfs_open(progname, O_RDONLY, 0, &v);
	// if (result) {
	// 	*errptr = result;
	// 	return -1;
	// }

	// /* We DONT care about being a new process. We care about that only when we're forking. The default version of runprogram assumes that only fork will call it */
	// //KASSERT(proc_getas() == NULL);

	// /* Create a new address space. */
	// as = as_create();
	// if (as == NULL) {
	// 	vfs_close(v);
	// 	*errptr = ENOMEM;
	// 	return -1;
	// }

	// /* Switch to it and activate it. */
	// proc_setas(as);
	// as_activate();

	// /* Load the executable. */
	// result = load_elf(v, &entrypoint);
	// if (result) {
	// 	/* p_addrspace will go away when curproc is destroyed */
	// 	vfs_close(v);
	// 	*errptr = result;
	// 	return -1;
	// }

	// /* Done with the file now. */
	// vfs_close(v);

	// /* Define the user stack in the address space */
	// result = as_define_stack(as, &stackptr);
	// if (result) {
	// 	/* p_addrspace will go away when curproc is destroyed */
	// 	*errptr = result;
	// 	return -1;
	// }

	// /* Warp to user mode. */
	// enter_new_process(0 /*argc*/, NULL /*userspace addr of argv*/,
	// 		  NULL /*userspace addr of environment*/,
	// 		  stackptr, entrypoint);

	// /* enter_new_process does not return. */
	// panic("enter_new_process returned\n");
	// *errptr = EINVAL;
		return -1;
}




