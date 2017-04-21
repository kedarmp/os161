#include <sys_sbrk.h>
#include <current.h>


void * sys_sbrk(intptr_t amount,int *err)
{
	// lock_acquire(curproc->proc_lock);
	// kprintf("I am:%d, My parent:%d:\n",curproc->proc_id, curproc->parent_proc_id);
	struct addrspace *as = proc_getas();
	
	// kprintf("heap_start:%d \n",as->heap_region->start);
	// kprintf("heap_end:%d \n",as->heap_region->end);
	// kprintf("amount:%ld \n",amount);
	if(as == NULL)
	{
		*err = EFAULT;
		// lock_release(curproc->proc_lock);
		return (NULL);
	}
	if(amount == 0)
	{
		*err = 0;
		// lock_release(curproc->proc_lock);
		return ((void *)as->heap_region->end);
	}
	if(amount % PAGE_SIZE != 0)
	{
		*err = EINVAL;
		// lock_release(curproc->proc_lock);
		return NULL;
	}
	//-1073741824
	if(amount < 0)
	{

		//KASSERT(amount == -1073741824);	
		
		if((int)as->heap_region->end + amount < (int)as->heap_region->start)
		{
			*err = EINVAL;
			// lock_release(curproc->proc_lock);
			return NULL;
		}
		// else if(amount == -107 374 1824) {
		// 	*err = EINVAL;
		// 	return NULL;
		// }
		else
		{
			vaddr_t heap_ret = as->heap_region->end;
			as->heap_region->end+=amount;
			
			
			// free these many pages
			int num_pages_to_free = (-amount)/PAGE_SIZE;
			paddr_t page_addr =  as->heap_region->end;
			for(int i=0;i<num_pages_to_free;i++) {
				delete_pte(as,page_addr);
				page_addr = page_addr + PAGE_SIZE;
	

			}

			*err = 0;
			// lock_release(curproc->proc_lock);
			return ((void *)heap_ret);
		}
	}
	if(amount > 0)
	{
		if(as->heap_region->end + amount >= as->stack_region->start)
		{
			*err = ENOMEM;
			// lock_release(curproc->proc_lock);
			return NULL;
		}
		else
		{
			vaddr_t heap_ret = as->heap_region->end;
			as->heap_region->end += amount;
			*err = 0;
			// lock_release(curproc->proc_lock);
			return ((void *)heap_ret);
		}
	}
	// lock_release(curproc->proc_lock);
	return NULL;
}