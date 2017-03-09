#include <sys_execv.h>
#include <thread.h>
#include <copyinout.h>

char ev_buff[ARG_MAX];
void load_kernel_buffer(char **args,int n_args);

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
			//kfree above stuff
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
		
	 struct addrspace *as;
	 struct vnode *v;
	 vaddr_t entrypoint, stackptr;
	 int result;

	 

	 /* Open the file. */
	 result = vfs_open(progname, O_RDONLY, 0, &v);
	 if (result) {
	 	*errptr = result;
	 	return -1;
	 }

	 /* We DONT care about being a new process. We care about that only when we're forking. The default version of runprogram assumes that only fork will call it */
	 //KASSERT(proc_getas() == NULL);

	 /* Create a new address space. */
	 as = as_create();
	 if (as == NULL) {
	 	vfs_close(v);
	 	*errptr = ENOMEM;
	 	return -1;
	 }

	 /* Switch to it and activate it. */
	 proc_setas(as);
	 as_activate();

	 /* Load the executable. */
	 result = load_elf(v, &entrypoint);
	 if (result) {
	 	/* p_addrspace will go away when curproc is destroyed */
	 	vfs_close(v);
	 	*errptr = result;
	 	return -1;
	 }

	 /* Done with the file now. */
	 vfs_close(v);

	 /* Define the user stack in the address space */
	 result = as_define_stack(as, &stackptr);
	 if (result) {
	 	/* p_addrspace will go away when curproc is destroyed */
	 	*errptr = result;
	 	return -1;
	 }

	load_kernel_buffer(args,n_args);
	 

	 /* Warp to user mode. */
	 enter_new_process(0 /*argc*/, NULL /*userspace addr of argv*/,
	 		  NULL /*userspace addr of environment*/,
	 		  stackptr, entrypoint);

	 /* enter_new_process does not return. */
	 panic("enter_new_process returned\n");
	 *errptr = EINVAL;
		return -1;
}

void load_kernel_buffer(char **args,int n_args) {
	bzero(ev_buff,ARG_MAX);
	int start_address = 4*n_args;
	int i = 0;
	int j = 0;
	int offset = start_address;

	for(i=0;i<n_args;i++)
	{
		j = 0;
		while(args[i][j++]!='\0');	//length of arg i
		memcpy(ev_buff+offset,args[i],j-1);	//doesnt matter if we write j or j-1
		ev_buff[offset+j] = '\0';
		offset += j;
		if((offset % 4) != 0)
		{
			int append_null = 4 - (offset % 4);
			int traverse;
			for(traverse = 0; traverse < append_null; traverse++)
			{
				ev_buff[offset+traverse] = '\0';
			}
			offset+=append_null;
		}
	}

	//10 + 6 incl null true.
	//start_address is the first available position that can be used to put values in

	for(i=start_address;i<offset;i++)
	{
		kprintf("Val: %c\n",(ev_buff[i]));
	}

	char *ptrs[n_args];
	char *stack_base = (char*)0x80000000;
	int end_ptr = start_address;

	kprintf("stack base:%u\n",(int)stack_base);	//decimal for convenience
	//setup pointers
	
	char *arg_base = stack_base - (offset - start_address);	//first location from which arguments start and below which pointers (will) reside
	kprintf("arg base:%u\n",(int)arg_base);

	int arglen = 0; int index=0;
	// kprintf("start-address:%d,offset:%d\n",start_address,offset);
	for(i=0;i<n_args;i++) {
		//find end of ith argument
		arglen=0;
		while(ev_buff[end_ptr + arglen++]!='\0');	//first null
		index = 0;
		while(1) {
			index = end_ptr + arglen++;
			while(index<offset && ev_buff[index]=='\0') {	//second null
				index = end_ptr + arglen++;
			}
			break;
		}
		arglen--;

		kprintf("Arg %d in ev_buff is %d bytes long\n",i,arglen);

		end_ptr += arglen;
		ptrs[i] = arg_base;
		arg_base += arglen;	//update arg_base
		
		kprintf("Address of ith arg:%u\n",(int)ptrs[i]);
	}

	
	//copy pointers to bottom of stack
	char* stack_bottom = stack_base - offset;
	kprintf("offset:%d\n",offset);
	kprintf("stack_bottom before copying ptrs:%u\n",(int)stack_bottom);
	for(i=0; i<n_args; i++) {	//copy pointers including the NULL pointer
		kprintf("copying pointer:%u to stack\n",(int)ptrs[i]);
		memcpy(stack_bottom,ptrs[i],4);
		stack_bottom+=4;
	}
	kprintf("stack_bottom before args:%u\n",(int)stack_bottom);

	//copy arguments

	int stack_offset = 0;
	for(i=0;i<n_args;i++)
	{
		j = 0;
		while(args[i][j++]!='\0');	//length of arg i
		memcpy(stack_bottom + stack_offset,args[i], j-1);	//doesnt matter if we write j or j-1
		stack_bottom[stack_offset+j] = '\0';
		stack_offset += j;
		if((stack_offset % 4) != 0)
		{
			int append_null = 4 - (stack_offset % 4);
			int traverse;
			for(traverse = 0; traverse < append_null; traverse++)
			{
				stack_bottom[stack_offset + traverse] = '\0';
			}
			stack_offset += append_null;
		}
	}

	kprintf("stack_offset:%d\n",stack_offset);
	kprintf("stack_bottom:%u\n",(int)stack_bottom);
	stack_bottom = stack_base - offset;
	kprintf("stack_bottom:%u",(int)stack_bottom);
	//print stack arguments
	for(i=start_address;i<offset;i++)
	{
		kprintf("Val: %c\n",(stack_bottom[i]));
	}

	void *arg0ptr = kmalloc(4);
	memcpy(arg0ptr, stack_bottom, 4);
	//try to print value via pointer stored in stack
	kprintf("val of ptr0:%s\n",(char*)arg0ptr);



	//add a NULL pointer - how?


}




