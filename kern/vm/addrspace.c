/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <addrspace.h>
#include <vm.h>
#include <proc.h>
#include <current.h>
#include <mips/tlb.h>
 #include <spl.h>


//#include <kern/vm.h>


extern vaddr_t firstfree;
struct spinlock core_lock;


/*
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 */

struct core_entry* coremap;
//paddr_t first_free;
unsigned int used_bytes;
int total_pages;


void update_heap(struct addrspace *as, vaddr_t new_heap_start);
int check_valid_seg(vaddr_t faultaddress ,struct addrspace *as);
struct pte* check_in_page_table(vaddr_t faultaddress,struct addrspace *as);
struct pte* create_pte(vaddr_t faultaddress, struct addrspace *as);

void vm_bootstrap(void) {

}

int check_valid_seg(vaddr_t faultaddress ,struct addrspace *as)
{
	//0 - not found
	//1 - found 
	if(faultaddress == (vaddr_t)NULL)
	{
		return 0;
	}
	struct region* mover = as->a_regions;
	while(mover!=NULL)
	{
		if(faultaddress >= mover->start && faultaddress < mover->end)
		{
			return 1;
		}
		mover = mover->next;
	}
	
	//CHECK - STACK 
	if(faultaddress >= as->stack_region->start && faultaddress < (as->stack_region->end))
	{
		return 1;
	}

	//CHECK HEAP 
	if(faultaddress >= as->heap_region->start && faultaddress < as->heap_region->end)
	{
		return 1;
	}

	return 0;
}

struct pte* check_in_page_table(vaddr_t faultaddress,struct addrspace *as)
{
	//0 - not found
	//1 - found 
	if(faultaddress == (vaddr_t)NULL)
	{
		return NULL;
	}
	vaddr_t vpn = trim_virtual(faultaddress);

	struct pte* mover = as->page_table;

	while(mover!=NULL)
	{
		if(vpn == mover->vpn)
		{
			return mover;
		}
		mover = mover->next;
	}
	return NULL;
}

struct pte* create_pte(vaddr_t faultaddress, struct addrspace *as)
{
	//Do we have to update stack pointer(stack->end in our case) 
	
	paddr_t new_p_page = alloc_upage();

	struct pte *mover = as->page_table;
	if(mover==NULL) {
		//first region
		as->page_table = kmalloc(sizeof(struct pte));
		if(as->page_table == NULL) {
			//UNDO EVERYTHING DONE BEFORE -> ALL KMALLOCS
			return NULL;
		}
		as->page_table->vpn = trim_virtual(faultaddress);
		as->page_table->ppn = trim_physical(new_p_page);
		as->page_table->next = NULL;
		as->page_table->state = PTE_IN_MEMORY;
		return as->page_table;
	}
	else {
		while(mover->next!=NULL) {
			mover = mover->next;

			}
		struct pte *new_pte = kmalloc(sizeof(struct pte));
		if(new_pte == NULL) {
			//undo all kmallocs above
			return NULL;
		}
		new_pte->vpn = trim_virtual(faultaddress);
		new_pte->ppn = trim_physical(new_p_page);
		new_pte->next = NULL;
		new_pte->state = PTE_IN_MEMORY;
		mover->next = new_pte;
		return new_pte;			
	}
	return NULL;
}


/* Fault handling function called by trap code */
int vm_fault(int faulttype, vaddr_t faultaddress) {
	(void)faulttype;
	int spl;
	if(curproc == NULL)
	{
		return EFAULT;
	}
	struct addrspace *as = proc_getas();
	if(as == NULL)
	{
		return EFAULT;
	}
	int ret = check_valid_seg(faultaddress,as);
	if(ret == 0)
	{
		return EFAULT;
	}

	struct pte *page = check_in_page_table(faultaddress,as);
	if(page == NULL)
	{
		//Create a new PTE
		page = create_pte(faultaddress, as);
		if(page == NULL) {
			return EFAULT;
		}

	}
	//check if a physical page has been allocated, if not, allocate it
	KASSERT(page->state == PTE_IN_MEMORY);
	if(page->state == PTE_IN_MEMORY)
	{
		//UPDATE TLB
		spl = splhigh();
		tlb_random(page->vpn, (page->ppn) | TLBLO_DIRTY | TLBLO_VALID);
		splx(spl);

	}
	// else
	// {
	// 	//SWAPPING 
	// }
	return 0;
}

/* Allocate/free kernel heap pages (called by kmalloc/kfree) */
vaddr_t alloc_kpages(unsigned npages) {

		spinlock_acquire(&core_lock);	
		struct core_entry* traverse_end = coremap;
		struct core_entry* marker = traverse_end;
		
		struct core_entry e;
		
		unsigned found = 0;
		vaddr_t freepage = firstfree;

		int i;
		for( i=0; i<total_pages; i++) {
			e = traverse_end[i];
			if(e.state == PAGE_FREE) {
				
				found++;
				if(found == npages) {
					break;
				}
			}
			else {
				if(found!=npages) {
					found = 0;
				}
			}
			
			if(found==0) {	//we will start fresh. Marker should always point to the first struct of our desired "contiguous segment"
				marker = traverse_end+i+1;
			 	freepage = MIPS_KSEG0 + PAGE_SIZE*(i+1); //update physical address too
			}
		}
		if(i==total_pages)
		{
			spinlock_release(&core_lock);	
			return (vaddr_t)NULL;
		}
			
		if(found == npages) {	//found "npages" free pages starting at traverse
			//paddr_t ret_address = traverse_end - (npages*sizeof(struct core_entry));

			//the segment from marker upto npages ahead should now be marked as fixed
			
			for(unsigned j=0; j<npages; j++) {
				marker[j].state = PAGE_FIXED;
				if(j==0)
					marker[j].chunk_size = npages;	//else its zero
			}
			used_bytes += npages*PAGE_SIZE;
			//return a physical address vaddr corresponding to "marker"
			// kprintf("Allocating %d pages from %u\n",npages,freepage);
			spinlock_release(&core_lock);
			return freepage;
		}
		

	// return (vaddr_t)NULL;
	spinlock_release(&core_lock);
	return (vaddr_t)NULL;

}
void free_kpages(vaddr_t addr) {
	// kprintf("free vaddr:%u\n",addr);
	//make all kinds of validity checks.e.g make sure tha addr>MIPS_KSEG0 for a kernel page
	spinlock_acquire(&core_lock);
	int index = (addr - MIPS_KSEG0)/PAGE_SIZE;
	struct core_entry e = coremap[index];
	// kprintf("struct located. Chunk size:%d\n",e.chunk_size);
	if(e.state == PAGE_FIXED && e.chunk_size!=0) {	//later we shouldnt be freeing FIXED pages
		used_bytes-= (e.chunk_size * PAGE_SIZE);
		for(int i=0;i<e.chunk_size;i++) {
			coremap[index+i].state = PAGE_FREE;
			coremap[index+i].chunk_size = 0;
		}	
	}
	spinlock_release(&core_lock);
}



// ALloc user page:

/* Allocate/free kernel heap pages (called by kmalloc/kfree) */
paddr_t alloc_upage(void) {

		spinlock_acquire(&core_lock);	
		struct core_entry* traverse_end = coremap;		
		struct core_entry e;
		
		paddr_t free_p_page = firstfree;

		int i;
		for( i=0; i<total_pages; i++) {
			e = traverse_end[i];
			if(e.state == PAGE_FREE) {
				free_p_page =  PAGE_SIZE*(i); //update physical address too
				traverse_end[i].state = 30;	//can be swapped out in 3.3
				traverse_end[i].chunk_size = 1;	//else its zero
				used_bytes += PAGE_SIZE;
				break;
			}

		}
		if(i==total_pages)
		{
			spinlock_release(&core_lock);	
			return (paddr_t)NULL;
		}
		
		spinlock_release(&core_lock);
		bzero((void*)(free_p_page+MIPS_KSEG0), PAGE_SIZE);
		return (free_p_page);
}

void free_upage(paddr_t addr) {
	spinlock_acquire(&core_lock);
	int index = (addr)/PAGE_SIZE;
	struct core_entry e = coremap[index];
	// kprintf("struct located. Chunk size:%d\n",e.chunk_size);
	KASSERT(e.state == 30);
	KASSERT(e.chunk_size != 0);
	if(e.state == 30 && e.chunk_size!=0) {	//later we shouldnt be freeing FIXED pages
		//kprintf("Alloc_U_PAGES:Found \n");
		used_bytes-= PAGE_SIZE;
		coremap[index].state = PAGE_FREE;
		coremap[index].chunk_size = 0;
	}
	spinlock_release(&core_lock);
}





/*
 * Return amount of memory (in bytes) used by allocated coremap pages.  If
 * there are ongoing allocations, this value could change after it is returned
 * to the caller. But it should have been correct at some point in time.
 */
unsigned int coremap_used_bytes(void) {
	return used_bytes;
}

/* TLB shootdown handling called from interprocessor_interrupt */
void vm_tlbshootdown(const struct tlbshootdown * t) {
	(void)t;
}

















 //----------------------//


//different functions for clarity
paddr_t trim_physical(paddr_t original) {
	// original = original >>12;
	// original = original <<12;
	return (original & 0xfffff000);
}
vaddr_t trim_virtual(vaddr_t original) {
	// original = original >>12;
	// original = original <<12;
	return (original & 0xfffff000);
}


void update_heap(struct addrspace *as, vaddr_t new_heap_start) {
	as->heap_region->start = new_heap_start;
	as->heap_region->end = new_heap_start;
}


struct addrspace *
as_create(void)
{
	struct addrspace *as;

	as = kmalloc(sizeof(struct addrspace));
	if (as == NULL) {
		return NULL;
	}
	as->a_regions = NULL;
	as->stack_region = kmalloc(sizeof(struct region));
	if(as->stack_region == NULL)
		return NULL;
	as->stack_region->next = NULL;
	
	as->heap_region = kmalloc(sizeof(struct region));
	if(as->heap_region == NULL)
		return NULL;
	as->heap_region->next = NULL;
	as->total_region_end = 0;
	as->page_table = NULL;

	return as;
}


void
as_destroy(struct addrspace *as)
{
	if(as!=NULL) {
		//deallocate a_regions linkedlist
		if(as->a_regions != NULL) {
			struct region * mover1 = as->a_regions;
			struct region *mover2 = NULL;
			while(mover1!=NULL) {
				mover2 = mover1->next;
				kfree(mover1);
				mover1 = mover2;
			}
		}
		//deallocate stack and heap regions
		if(as->stack_region!=NULL)
			kfree(as->stack_region);
		if(as->heap_region!=NULL)		//where to call free_upage?
			kfree(as->heap_region);

		//deallocate page table
		if(as->page_table != NULL) {
			struct pte *mover1 = as->page_table;
			struct pte *mover2 = NULL;
			while(mover1 != NULL) {
				mover2 = mover1->next;
				//free physical page
				free_upage( mover1->ppn);
				kfree(mover1);
				mover1 = mover2;
			}
		}
		kfree(as);
	}
}

int
as_copy(struct addrspace *old, struct addrspace **ret)
{
	struct addrspace *newas;

	newas = as_create();
	if (newas==NULL) {
		return ENOMEM;
	}


	//copy regions
	struct region * mover_old = old->a_regions;
	struct region * mover_new = NULL;

	while(mover_old!=NULL) {
		struct region *next_region = kmalloc(sizeof(struct region));
		if(next_region == NULL) {
			return ENOMEM;
		}
		if(newas->a_regions == NULL) {
			newas->a_regions = next_region;
		}
		if(mover_new != NULL) {
			mover_new->next = next_region;
		}
		next_region->start = mover_old->start;
		next_region->end = mover_old->end;	
		next_region->next = NULL;
		mover_old = mover_old->next;
		
		mover_new = next_region;
	}
	//copy stack and heap
	newas->stack_region->start = old->stack_region->start;
	newas->stack_region->end = old->stack_region->end;
	//copy stack contents
	// memmove((void *)(newas->stack_region->start),(const void*)(old->stack_region->start),CUSTOM_STACK_SIZE);

	newas->heap_region->start = old->heap_region->start;
	newas->heap_region->end = old->heap_region->end;

	// newas->page_table = kmalloc(sizeof(struct pte));
	//copy ptes
	struct pte * pte_old = old->page_table;
	struct pte * mover = NULL;


	while(pte_old!=NULL) {
		struct pte *pte_new = kmalloc(sizeof(struct pte));
		if(pte_new == NULL) {
			return ENOMEM;
		}
		if(newas->page_table == NULL) {
			newas->page_table = pte_new;
		}
		if(mover!=NULL) {
			mover->next = pte_new;
		}

		pte_new->next = NULL;
		pte_new->vpn = pte_old->vpn;
		pte_new->state = PTE_IN_MEMORY;
		//allocate new physical page for this pte
		paddr_t new_p_page= alloc_upage();
		pte_new->ppn = trim_physical(new_p_page);

		//copy contents of old physical page to new physical page
		memmove((void *)(MIPS_KSEG0 + pte_new->ppn),(const void*)(MIPS_KSEG0 + pte_old->ppn),PAGE_SIZE);
	
		pte_old = pte_old->next;
		mover = pte_new;
	}

	*ret = newas;
	return 0;
}



//simply copied from dumbvm.c(reused as carl says)
void
as_activate(void)
{
	int i, spl;
	struct addrspace *as;

	as = proc_getas();
	if (as == NULL) {
		return;
	}

	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

	for (i=0; i<NUM_TLB; i++) {
		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}

	splx(spl);
}

void
as_deactivate(void)
{
	/*
	 * Write this. For many designs it won't need to actually do
	 * anything. See proc.c for an explanation of why it (might)
	 * be needed.
	 */	
}

/*
 * Set up a segment at virtual address VADDR of size MEMSIZE. The
 * segment in memory extends from VADDR up to (but not including)
 * VADDR+MEMSIZE.
 *
 * The READABLE, WRITEABLE, and EXECUTABLE flags are set if read,
 * write, or execute permission should be set on the segment. At the
 * moment, these are ignored. When you write the VM system, you may
 * want to implement them.
 */
int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t memsize,
		 int readable, int writeable, int executable)
{
	//do we need to check if vaddr is page aligned? dumbvm does that

	struct region *mover = as->a_regions;
	if(as->a_regions==NULL) {
		//first region
		as->a_regions = kmalloc(sizeof(struct region));
		if(as->a_regions == NULL) {
			return ENOSYS;
		}
		as->a_regions->start = vaddr;
		as->a_regions->end = vaddr + memsize;
		as->a_regions->next = NULL;
		as->total_region_end = vaddr+memsize;
	}
	else {
		while(mover->next!=NULL) {
			mover = as->a_regions->next;
			}
		struct region *new_region = kmalloc(sizeof(struct region));
		if(new_region == NULL) {
			return ENOSYS;
		}
		new_region->start = vaddr;
		new_region->end = vaddr + memsize;
		new_region->next = NULL;
		mover->next = new_region;
		if(vaddr+memsize > as->total_region_end) {
			as->total_region_end = vaddr+memsize;
		}			
	}
	
	update_heap(as, as->total_region_end);


	(void)readable;
	(void)writeable;
	(void)executable;
	return 0;
}

int
as_prepare_load(struct addrspace *as)
{
	/*
	 * Write this.
	 */

	(void)as;
	return 0;
}

int
as_complete_load(struct addrspace *as)
{
	/*
	 * Write this.
	 */

	(void)as;
	return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
	as->stack_region->start = USERSTACK - CUSTOM_STACK_SIZE;
	as->stack_region->end = USERSTACK;

	/* Initial user-level stack pointer */
	*stackptr = USERSTACK;

	return 0;
}