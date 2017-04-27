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

#include <vnode.h>
#include <vfs.h>
#include <stat.h>
#include <bitmap.h>
#include <uio.h>
#include<cpu.h>

int clock_hand;


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
struct lock *bitmap_lock;


void update_heap(struct addrspace *as, vaddr_t new_heap_start);
int check_valid_seg(vaddr_t faultaddress ,struct addrspace *as);
struct pte* check_in_page_table(vaddr_t faultaddress,struct addrspace *as);
struct pte* create_pte(vaddr_t faultaddress, struct addrspace *as);

int evict_page(uint32_t max);
void swapout(int idx, paddr_t addr, struct pte* old_pte, struct pte* new_pte, int decision);
void swapin(paddr_t free_page, struct pte * page_pte);

struct bitmap *map;
struct vnode *swap_file;
int write_to_disk(paddr_t content, off_t disk_offset); 
int read_to_disk(paddr_t targetaddr, off_t disk_offset);

int SWAP_ENABLED;

int last = -1;

void vm_bootstrap(void) {
	//initialize bitmap

	SWAP_ENABLED = 0;
	//Try to open swap file and see its size..?
	char swapfile[] = "lhd0raw:";
	int err = vfs_open(swapfile, O_RDWR, 0, &swap_file);
	if(err) {
		kprintf("Error opening swapfile:%d\n",err);
		return;
	}
	SWAP_ENABLED = 1;
	struct stat swap_stats;
	VOP_STAT(swap_file, &swap_stats);
	kprintf("Size of swap file:%lld\n",swap_stats.st_size);
	map = bitmap_create(swap_stats.st_size/PAGE_SIZE);
	KASSERT(map != NULL);	//else swapping wont work
	bitmap_lock = lock_create("bmlock");
	clock_hand = 0;	//should ideally be first user page. (exclude kernel page always. see ram.c)

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
		//lock_acquire(mover->pte_lock);
		if(vpn == mover->vpn)
		{
			return mover;	//NOTE: After this point, we will either go to PTE_IN_MEMORY or ON_DISK. If it is on disk, we will call alloc_upage, which calls swapout, which will
			//try to reacquire this same lock - which is fine in our implementation of locks. If it is in memory, we shall make sure to release it there manually
		}
	//	lock_release(mover->pte_lock);
		mover = mover->next;
	}
	return NULL;
}

struct pte* create_pte(vaddr_t faultaddress, struct addrspace *as)
{
	//Do we have to update stack pointer(stack->end in our case) 
	
	struct pte *new_pte = kmalloc(sizeof(struct pte));
	if(new_pte == NULL) {
		return NULL;
	}
//	kprintf("create:%p\n",new_pte);
	
	new_pte->vpn = trim_virtual(faultaddress);
	new_pte->next = NULL;
	new_pte->state = PTE_IN_MEMORY;
	new_pte->disk_offset = -1;
	new_pte->pte_lock = lock_create("pte_lock");

	lock_acquire(new_pte->pte_lock);
	paddr_t new_p_page = alloc_upage(new_pte, WILL_BE_FOLLOWED_BY_SWAPIN);
	
	spinlock_acquire(&core_lock);
	coremap[new_p_page/PAGE_SIZE].pte_ptr = new_pte;
	spinlock_release(&core_lock);

	KASSERT(new_p_page != (paddr_t)NULL);
	KASSERT(coremap[new_p_page/PAGE_SIZE].pte_ptr == new_pte);
	new_pte->ppn = trim_physical(new_p_page);

	struct pte *mover = as->page_table;
	if(mover==NULL) {
		//first region
		as->page_table = new_pte; 
	}
	else {
		while(mover->next!=NULL) {
			mover = mover->next;

			}
		mover->next = new_pte;
	}
//	lock_acquire(new_pte->pte_lock);	//acquire lock immediately after creation so that the later code can run atomically without allowing hthis page to bedestroyed
	spinlock_acquire(&core_lock);
	coremap[new_p_page/PAGE_SIZE].state = PAGE_USER;
	spinlock_release(&core_lock);
	return new_pte;
}

void delete_pte(struct addrspace *as, vaddr_t addr) {
	struct pte * mover = as->page_table;
	struct pte *mover2 = mover;
	
	while(mover!=NULL) {
		if(mover->vpn == (addr)) {
			mover2->next = mover->next;
			
			lock_acquire(mover->pte_lock);
			if(mover->state == PTE_IN_MEMORY) {
			//clear the tlb
			int spl = splhigh();
			int tlb_idx = tlb_probe(mover->vpn, 0);
			if(tlb_idx>=0) {
				tlb_write(TLBHI_INVALID(tlb_idx), TLBLO_INVALID(), tlb_idx);
			}
			splx(spl);

			free_upage(mover->ppn);
			}
			else if(mover->state == PTE_ON_DISK) {
                                //unmark the bitmap on disk, and bzero the disk somehow
                                lock_acquire(bitmap_lock);
                                bitmap_unmark(map, (mover->disk_offset)/PAGE_SIZE);
                                //Do we need to clear the disk area too? How?
                                lock_release(bitmap_lock);
                        } else {
                                panic("Invalid PTE state!\n");
                        }
                        lock_release(mover->pte_lock);
			lock_destroy(mover->pte_lock);
			mover->pte_lock = NULL;
			kfree(mover);
			return;
		}
		mover2 = mover;
		mover = mover->next;
	}
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
			return ENOMEM;	//one of the mallocs failed in create_pte
		}

	}
	lock_acquire(page->pte_lock);
	//check if a physical page has been allocated, if not, allocate it
	if(page->state == PTE_IN_MEMORY) {
		//UPDATE TLB
		spl = splhigh();
		tlb_random(page->vpn, (page->ppn) | TLBLO_DIRTY | TLBLO_VALID);
		splx(spl);
		spinlock_acquire(&core_lock);
		coremap[page->ppn/PAGE_SIZE].color = PTE_GREY;
		spinlock_release(&core_lock);
		//release the lock acquired when we find a pte in the page table
		lock_release(page->pte_lock);

	}
	else if(page->state == PTE_ON_DISK){
	 	//swap in the page 
		paddr_t free_page = alloc_upage(page, WILL_BE_FOLLOWED_BY_SWAPIN);
		spinlock_acquire(&core_lock);
		coremap[free_page/PAGE_SIZE].pte_ptr = page;
		spinlock_release(&core_lock);
		page->ppn = free_page;
		KASSERT(coremap[free_page/PAGE_SIZE].pte_ptr == page);
		swapin(free_page, page);

	} else {
		panic("Invalid page state");
	}
	// ipi_broadcast(IPI_TLBSHOOTDOWN);
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
		if(found == npages) {	//found "npages" free pages starting at traverse
			//paddr_t ret_address = traverse_end - (npages*sizeof(struct core_entry));

			//the segment from marker upto npages ahead should now be marked as fixed
			
			for(unsigned j=0; j<npages; j++) {
				marker[j].state = PAGE_FIXED;
				marker[j].pte_ptr = NULL;	//No PTE for kernel page
				if(j==0)
					marker[j].chunk_size = npages;	//else its zero
			}
			used_bytes += npages*PAGE_SIZE;
			//return a physical address vaddr corresponding to "marker"
			// kprintf("Allocating %d pages from %u\n",npages,freepage);
			bzero((void*)(freepage),PAGE_SIZE);
			
			spinlock_release(&core_lock);
			return freepage;
		}

		if(i==total_pages)
		{
			if(SWAP_ENABLED == 1) {
			//choose page to evict
			KASSERT(npages == 1);
			int idx = evict_page(total_pages);
			paddr_t evicted_paddr = PAGE_SIZE*idx;
			struct pte *old_pte = coremap[idx].pte_ptr;

			KASSERT(old_pte != NULL);
			
			KASSERT(old_pte->ppn == evicted_paddr);	//only if the page is  in memory should we perform this basic check
			


			coremap[idx].state = PAGE_SWAPPING;
			coremap[idx].chunk_size = 1;
			coremap[idx].pte_ptr = NULL;	
			
			spinlock_release(&core_lock);	

			lock_acquire(old_pte->pte_lock);
			old_pte->state = PTE_ON_DISK;
			// lock_release(old_pte->pte_lock);
			swapout(idx, evicted_paddr, old_pte, NULL, WILL_NOT_BE_FOLLOWED_BY_SWAPIN);
			return (vaddr_t)(MIPS_KSEG0 + evicted_paddr); 
			}
			else {
			spinlock_release(&core_lock);	
			return (vaddr_t)NULL;
			
			}
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
			coremap[index].pte_ptr = NULL;
		}	
	}
	spinlock_release(&core_lock);
}

//return random number between 0 to (max-1)
int evict_page(uint32_t max) {
/*	int random_idx = random()%max;
	//Dont return us fixed/in-swap pages at all
	while(coremap[random_idx].state == PAGE_FIXED || coremap[random_idx].state == PAGE_SWAPPING || coremap[random_idx].state == PAGE_COPYING) {
		random_idx = random()%max;
	}
	return random_idx;
*/
 	//  Round robin
	// unsigned int howmany = 0;
	// unsigned int i=(last+1)%max;
	// for(;howmany<max;i++,howmany++) {
	// 	if(coremap[i].state == PAGE_USER) {
	// 		last = i;
	// 		break;
	// 	}
	// 	if(i == max-1) {
	// 		i=0;
			
	// 	}
	// }
	// return last;

	//Clock 
	unsigned int i=(last+1)%max;
	for(;;i++) {
		if(coremap[i].state == PAGE_USER) {
			if(coremap[i].color == PTE_GREY) {
				coremap[i].color = PTE_GREEN;
			}
			else {
				//found a green slot. Evict this page!
				last = i;
				break;
			}

		}
		if(i == max-1) {
			i=0;
			
		}
	}
	return last;
	
	

}

// ALloc user page:

/* Allocate/free kernel heap pages (called by kmalloc/kfree) */
paddr_t alloc_upage(struct pte *page_pte, int decision) {

		KASSERT(page_pte > (struct pte*)0x80000000);
		KASSERT(page_pte != NULL);
		spinlock_acquire(&core_lock);	
		struct core_entry* traverse_end = coremap;		
		struct core_entry e;
		
		paddr_t free_p_page = firstfree;

		int i;
		for( i=0; i<total_pages; i++) {
			e = traverse_end[i];
			if(e.state == PAGE_FREE) {
				free_p_page =  PAGE_SIZE*(i); //update physical address too
				traverse_end[i].state = PAGE_USER;	//can be swapped out in 3.3
				traverse_end[i].chunk_size = 1;	//else its zero
				traverse_end[i].pte_ptr = page_pte;
				
				used_bytes += PAGE_SIZE;
				break;
			}

		}
		if(i==total_pages)
		{
			if(SWAP_ENABLED == 1) {
			//choose page to evict
			int idx = evict_page(total_pages);
			paddr_t evicted_paddr = PAGE_SIZE*idx;
			
			struct pte *old_pte = coremap[idx].pte_ptr;
			KASSERT(old_pte != NULL);	
			KASSERT(coremap[evicted_paddr/PAGE_SIZE].pte_ptr == old_pte);	
			KASSERT(old_pte->ppn == evicted_paddr);	//only if the page is  in memory should we perform this basic check
			
			coremap[idx].state = PAGE_SWAPPING;
			coremap[idx].chunk_size = 1;
			//coremap[idx].pte_ptr = page_pte;		//new_pte now owns this physical frame. If alloc_kpages calls swapout, new_pte will be NULL which is correct - there is no pte for a kernel allocation

			//4/23: if a curcpu->spinlock ASSERT fails, then move this lock block below the spinlock_release OR acquire this lock right before spinlock_acquire(make sure to release it at all points though..esp the break statements!)
			
			
			spinlock_release(&core_lock);

			lock_acquire(old_pte->pte_lock);
			old_pte->state = PTE_ON_DISK;
			// lock_release(old_pte->pte_lock);
			swapout(idx, evicted_paddr, old_pte, page_pte, decision);
			
			//spinlock_release(&core_lock);	
			return evicted_paddr;
			}
			else {
			spinlock_release(&core_lock);	
			return (paddr_t)NULL;
			}
		}
		
		bzero((void*)(free_p_page+MIPS_KSEG0), PAGE_SIZE);
		spinlock_release(&core_lock);
		return (free_p_page);
}

void free_upage(paddr_t addr) {
	spinlock_acquire(&core_lock);
	int index = (addr)/PAGE_SIZE;
	struct core_entry e = coremap[index];
	// kprintf("struct located. Chunk size:%d\n",e.chunk_size);
	//KASSERT(e.state == PAGE_USER);
	//KASSERT(e.chunk_size != 0);	
	if(e.state == PAGE_USER) {	//free only "user" pages
		//kprintf("Alloc_U_PAGES:Found \n");
		used_bytes-= PAGE_SIZE;
		coremap[index].state = PAGE_FREE;
		coremap[index].chunk_size = 0;
		coremap[index].pte_ptr = NULL;
	}
	spinlock_release(&core_lock);
}



//Clear the TLB, Swap out a page located  at addr and whose coremap entry is at index idx
void swapout(int idx, paddr_t addr, struct pte* old_pte, struct pte* new_pte, int decision) {
	
	// lock_acquire(old_pte->pte_lock);
	//flush TLB
	(void) decision;
	int spl = splhigh();
   	int tlb_idx = tlb_probe(old_pte->vpn & PAGE_FRAME, 0);
    if(tlb_idx>=0) {
		tlb_write(TLBHI_INVALID(tlb_idx), TLBLO_INVALID(), tlb_idx);
	}
	splx(spl);
	
	unsigned int pos=0;
	

	//KASSERT(old_pte->ppn == addr);
	off_t offset = old_pte->disk_offset;
	if(offset == -1) {
		//find location in bitmap
		lock_acquire(bitmap_lock);
		bitmap_alloc(map, &pos);
		lock_release(bitmap_lock);
		offset =  pos * PAGE_SIZE;

	}
	KASSERT(offset>=0);

	//else we simply write back to the same offset that this page was previously written to	
	
	//copy to file
	int err = write_to_disk(addr, offset);
	if(err) {
		panic("swapout: Couldn't write to disk!");
	}

	
	
	//copy the location to pte of of this page
	old_pte->disk_offset = offset;
	
	
	spinlock_acquire(&core_lock);	//may have to put this inside the pte lock?
	if(new_pte == NULL) {	//alloc_kpages called swapout
		coremap[idx].state = PAGE_FIXED;
	}

	bzero((void*)(addr + MIPS_KSEG0), PAGE_SIZE);
	//else, dont change swapping state until swapin completes!
	spinlock_release(&core_lock);
	lock_release(old_pte->pte_lock);

	//keep some kind of lock until this point above
	//else this lock will be released in swapin
}

//Reads a page in from the offset pointed to in page_pte into paddr: free_page and updates the PTE
//Also loads the TLB
void swapin(paddr_t free_page, struct pte * page_pte) {

//	kprintf("swapin called");
	//copy from swap file

	int err = read_to_disk(free_page, page_pte->disk_offset);
	if(err) {
		panic("swapin: Couldn't read from  disk!");
	}
	page_pte->state = PTE_IN_MEMORY;
	spinlock_acquire(&core_lock);

	//Load the TLB
	 int  spl = splhigh();
     tlb_random(page_pte->vpn, (page_pte->ppn) | TLBLO_DIRTY | TLBLO_VALID);
     splx(spl);		
	 coremap[page_pte->ppn/PAGE_SIZE].color = PTE_GREY;

	//update state of page in coremap
     
	
	KASSERT(coremap[free_page/PAGE_SIZE].pte_ptr == page_pte);

	coremap[free_page/PAGE_SIZE].state = PAGE_USER;
	spinlock_release(&core_lock);
	lock_release(page_pte->pte_lock);
    //release the lock acquired in swapout(when a vM-fault occured)

}

int write_to_disk(paddr_t content, off_t disk_offset) {
	struct uio u;
        struct iovec iov;
        uio_kinit(&iov,&u,(void*)PADDR_TO_KVADDR(content),PAGE_SIZE,disk_offset,UIO_WRITE);
        //perform the write!
        int err = VOP_WRITE(swap_file,&u);
        if(err!=0) {
                kprintf("Error writing:%d\n",err);
		return err;
        } else {
               // kprintf("Wrote successfully\n");
        }
	return 0;	
}

int read_to_disk(paddr_t targetaddr, off_t disk_offset) {
	struct uio u;
        struct iovec iov;
        uio_kinit(&iov,&u,(void*)PADDR_TO_KVADDR(targetaddr),PAGE_SIZE,disk_offset,UIO_READ);
        //perform the read!
        int err = VOP_READ(swap_file,&u);
        if(err!=0) {
                kprintf("Error reading:%d\n",err);
		return err;
        } else {
                //kprintf("Read successfully\n");
        }
	return 0;	
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
	//page align heap_start
	if(new_heap_start%PAGE_SIZE!=0) {
		int pages_needed = (new_heap_start/PAGE_SIZE)+1;
		new_heap_start = pages_needed*PAGE_SIZE;
	}

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
				lock_acquire(mover1->pte_lock);
				mover2 = mover1->next;
				//free physical page
				if(mover1->state == PTE_IN_MEMORY) {

					free_upage(mover1->ppn);
//If its PAGE_USER, then it nobody has swapped us out. if its swapping, then it's not our page anymore anyway. It cannot be anything else(fixed/copying/free) since we own it actively.
//					free_upage( mover1->ppn);
				}
				else if(mover1->state == PTE_ON_DISK) {
					//unmark the bitmap on disk, and bzero the disk somehow
					lock_acquire(bitmap_lock);
					bitmap_unmark(map, (mover1->disk_offset)/PAGE_SIZE);
					//Do we need to clear the disk area too? How?
					lock_release(bitmap_lock);
				} else {
					panic("Invalid PTE state!\n");
				}
				lock_release(mover1->pte_lock);
				lock_destroy(mover1->pte_lock);//NOTE: what happens to someone who has done an acquire on this lock?
				mover1->pte_lock = NULL;
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
		lock_acquire(pte_old->pte_lock);
		if(pte_old->state == PTE_IN_MEMORY) {
		spinlock_acquire(&core_lock);
		coremap[pte_old->ppn/PAGE_SIZE].state = PAGE_COPYING;
		spinlock_release(&core_lock);
		}
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
		pte_new->disk_offset = -1;
		pte_new->pte_lock = lock_create("ascopy_pte_lock");
		
		//allocate new physical page for this pte. This will lock the PTE (need to manually release the lock after copying is done)
		paddr_t new_p_page= alloc_upage(pte_new, WILL_BE_FOLLOWED_BY_SWAPIN);
		spinlock_acquire(&core_lock);
		coremap[new_p_page/PAGE_SIZE].pte_ptr = pte_new;
		spinlock_release(&core_lock);
//		KASSERT(coremap[new_p_page/PAGE_SIZE].pte_ptr == pte_new);
//this kassert fails (sometimes?) Perhaps it is because we access the cm out of the splock. But why?
		if(new_p_page==(paddr_t)NULL) {
			return ENOMEM;
		}
		pte_new->ppn = trim_physical(new_p_page);
		//copy contents of old physical page to new physical page
		
		if(pte_old->state == PTE_ON_DISK)
		{
			int err = read_to_disk(pte_new->ppn, pte_old->disk_offset);
			if(err) 
			{
				panic("ASCOPY: Couldn't read from disk!");
			}
		}
		else
		{
			//we need to make sure that the page corr to pte_old is NOT swapped out!
			memmove((void *)(MIPS_KSEG0 + pte_new->ppn),(const void*)(MIPS_KSEG0 + pte_old->ppn),PAGE_SIZE);
			spinlock_acquire(&core_lock);
			coremap[pte_old->ppn/PAGE_SIZE].state = PAGE_USER;
			spinlock_release(&core_lock);	
		}
		
		spinlock_acquire(&core_lock);
		coremap[pte_new->ppn/PAGE_SIZE].state = PAGE_USER;
		spinlock_release(&core_lock);

		lock_release(pte_old->pte_lock);
		
		
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
	// kprintf("memsize:%d \n",memsize);
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
