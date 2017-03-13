#include <sys_execv.h>
#include <thread.h>
#include <copyinout.h>
#include <kern/errno.h>

char ev_buff[ARG_MAX];
//int load_kernel_buffer(int n_args, userptr_t *argv_address,vaddr_t stack,int full_size);

//NEW ATTEMPT
int sys_execv(char* user_progname, char** user_args, int *errptr) {
	bzero(ev_buff,ARG_MAX);
	int i = 0, j = 0, n_args = 0, err = 0;
	
	char progname[PATH_MAX];
	size_t got;
	int full_size = 0;
	i=0;
	int ptr_len=0;

	//Progname null check
	if(user_progname == NULL)
	{
		*errptr = EFAULT;
		return -1;	
	}

	//Check if the args are null
	if(user_args == NULL)
	{
		*errptr = EFAULT;
		return -1;
	}

	//copy pointers
	while(user_args[i]!=NULL) {
	//copy pointers
		err = copyin((const_userptr_t)&user_args[i],(ev_buff+ptr_len),4);
		if(err) {
			*errptr = EFAULT;
			//kfree above stuff
			return -1;
		}
		ptr_len+=4;
		i++;
		n_args++;
		full_size+=4;
	}
	//Check if n_args is atleast ARG_MAX
	if(n_args > ARG_MAX)
	{
		*errptr = ENOMEM;
		return -1; 
	}
	//copy null pointer?
	// *(void**)(ev_buff+ptr_len) = NULL;
	// ptr_len++;
	ptr_len+=4;
	full_size+=4;

	//kprintf("number of args:%d \n",n_args);
	int arg_len=ptr_len;

	//copying strings

	i=0;
	while(i < n_args) {
		j=0;
		while(user_args[i][j++]!='\0')
			;
		int mod_str_len = 0;
		if(j%4 !=0 ) {
			mod_str_len = j+(4-(j%4));
		}
		else {
			mod_str_len = j;
		}
		// kprintf("@mod_str_len:%d\n",mod_str_len);
		// kprintf("copy to:%p (%u)\n",(void*)args_start + arg_end,args_start + arg_end);
		//kprintf("evbuff+arg:%p \n",ev_buff + arg_len);
		err = copyinstr((const_userptr_t)user_args[i],(ev_buff + arg_len),j,&got);
		if(err) {
			//kprintf("error copying arg\n");
			*errptr = EFAULT;
			return err;
			//kfree above stuff
		}
		arg_len+=j;
		int traverse;
		for(traverse = 0;traverse<(mod_str_len-j);traverse++)
		{
			*(ev_buff+arg_len+traverse) = '\0';
		}
		arg_len += traverse;
		full_size+=mod_str_len;
		i++;
	}

	//make pointers point to the right place
	void ** ptr = (void**) ev_buff;
	int args_start = 4*n_args+4;
	int padded_len = 0;
	int end_ptr = args_start;
	int index = 0;

	// for(i=0;i<full_size;i++) {
	// 	kprintf("%c \n",ev_buff[i]);
	// }
	//kprintf("fullsize:%d\n", full_size);

	for(i=0;i<n_args;i++) {
	// 	//find end of ith argument

		padded_len=0;
		while(ev_buff[end_ptr + padded_len++]!='\0');	//first null
		// kprintf("pL:%d\n",padded_len);
		index = 0;
			index = end_ptr + padded_len++;
			while(index<full_size && ev_buff[index]=='\0') {	//second null
				index = end_ptr + padded_len++;
			}
		padded_len--;		//length of arg+padding

//		kprintf("Arg %d in ev_buff is %d bytes long \n",i,padded_len);

		end_ptr += padded_len;
		ptr[i] = ev_buff + args_start;
		//kprintf("ptr[i]:%p \n",ev_buff + args_start);
		args_start += padded_len;	//update arg_base
		
		 //kprintf("Pointer %d points to %p:\n",i,ptrs[i]);
		// kprintf("Address of ith arg:%p\n",ptrs[i]);
	}
	// for(i=0;i<full_size;i++) {
	// 	if(ev_buff[i]=='\0')
	// 		//kprintf("@");
	// 	//kprintf("%c",ev_buff[i]);
	// }

	// kprintf("\n");

	// for(i=0;i<n_args;i++)
	// {
	// 	kprintf("ptr value : %s \n",(char *)ptr[i]);
	// }
	
	//old	

	// kprintf("FINAL LENGTH:%d\n",args_len);
	err = copyinstr((const_userptr_t)user_progname, progname, PATH_MAX, &got);
	if(err!=0) 
	{
		*errptr = EFAULT;
		return -1;	//or EFAULT?
	}
	if(got < 2 || got > PATH_MAX)
	{
		*errptr = EINVAL;
		return -1;
	}
	// kprintf("Programe name:%s\n",progname);	
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


	// int err;
	//kprintf("stackptr before :%p \n",(void*)stackptr);
	
	
	//Copying out pointers
	i = 0;
	void *buff = (void*) ev_buff;
	void** ev_buff_access1 = ((void**)buff);
	void** ev_buff_access2 = ((void**)buff);
	void* first = ev_buff_access1[0];
	void* stack_ptr = NULL;
	//kprintf("FullSize : %d \n",full_size);
	for(i = 0;i<n_args;i++)
	{
		//kprintf("val at ev_buff_access1 before:%p \n",ev_buff_access1[i]);
		int diff = (int) (ev_buff_access2[i] - first);
		stack_ptr = (stackptr - full_size + (4 * n_args + 4)) + (void*)diff;
		
	
		//kprintf("OFFSET:%d\n", diff);
		ev_buff_access1[i] = stack_ptr;
		//kprintf("val at ev_buff_access1 after:%p \n",ev_buff_access1[i]);
	}
	
	//Copying entire kernel buffer from stack bottom
	void *maybe = (void*)ev_buff;
	err = copyout((const void *)maybe,(userptr_t)(stackptr - full_size), full_size);
	if (err) {
	 	return err;
	}
	stackptr -= full_size;
	//kprintf("stackptr after :%p \n",(void*)stackptr);
	// userptr_t argv_start;
	// result = load_kernel_buffer(n_args, &argv_start,stackptr,full_size);
	// if (result) {
	//  	//kfree anything?
	//  	*errptr = result;
	//  	return -1;
	//  }

	// kprintf("argv_start:%p\n",argv_start);

	 /* Warp to user mode. */
	 enter_new_process(n_args, (userptr_t)stackptr,
	 		  NULL /*userspace addr of environment*/,
	 		  stackptr, entrypoint);

	 /* enter_new_process does not return. */
	 panic("enter_new_process returned\n");
	 *errptr = EINVAL;
		return -1;
}

// int load_kernel_buffer(int n_args, userptr_t *argv_address,vaddr_t stack,int full_size) {

// 	int err;
// 	vaddr_t stack_bottom = stack - full_size;

// 	//Copying out pointers
// 	int i = 0;

// 	void** ev_buff_access1 = ((void**)ev_buff);
// 	void** ev_buff_access2 = ((void**)ev_buff);
// 	void* stack_ptr = NULL;

// 	for(i = 0;i<n_args;i++)
// 	{
// 		stack_ptr = stack_bottom + ev_buff_access1[i];
// 		ev_buff_access2[i] = stack_ptr;
// 	}
	
// 	//Copying entire kernel buffer from stack bottom
// 	err = copyout((const void *)ev_buff,(userptr_t)stack_bottom,full_size);
// 	if (err) {
// 	 	return err;
// 	}

// 	memcpy(argv_address,&stack_bottom,4);
// 	return 0;
// }
