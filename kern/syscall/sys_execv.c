#include <sys_execv.h>
#include <thread.h>
#include <limits.h>

#include <copyinout.h>

// int sys_execv(const_userptr_t user_progname, const_userptr_t user_args, int *errptr) {
int sys_execv(char* user_progname, char** user_args, int *errptr) {

	kprintf("start");
	// kprintf("Pname:%s\n",user_progname);
	
	char progname[PATH_MAX];
	char *args[ARG_MAX];

	size_t got;
	int err = copyinstr((const_userptr_t)user_progname, progname, PATH_MAX, &got);
	if(err!=0) 
	{
		*errptr = err;
		return -1;	//or EFAULT?
	}
	
	kprintf("Programe name:%s",progname);
	
	// //arg checking
	// // int num_args = 0;
	//int i;
	// err = copyin((const_userptr_t)*user_args, args, ARG_MAX);
	// if(err!=0)
	// {
	// 	*errptr = err;
	// 	return -1;	//or EFAULT?
	// }
	kprintf("1st arg:%s",user_args[1]);
	copyin((const_userptr_t)user_args[1], args[1], PATH_MAX);//((tf->tf_a1))[i]
	
	// int i;
	// for(i=0;i<ARG_MAX && user_args[i]!=NULL;i++) {
	// 	// copyinstr((const_userptr_t)user_args[i], args[i], PATH_MAX, &got);//((tf->tf_a1))[i]
	// }

	kprintf("copyin passed");
	// for(i=0; i<ARG_MAX ;i++) 
	// {
	// 	kprintf("Arg %d : %s \n",i,args[i]);
	// }
	// (void)user_args;
	(void)errptr;
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




