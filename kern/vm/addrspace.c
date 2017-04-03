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

void vm_bootstrap(void) {

}

/* Fault handling function called by trap code */
int vm_fault(int faulttype, vaddr_t faultaddress) {
(void)faulttype;
(void)faultaddress;
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

struct addrspace *
as_create(void)
{
	struct addrspace *as;

	as = kmalloc(sizeof(struct addrspace));
	if (as == NULL) {
		return NULL;
	}

	/*
	 * Initialize as needed.
	 */

	return as;
}

int
as_copy(struct addrspace *old, struct addrspace **ret)
{
	struct addrspace *newas;

	newas = as_create();
	if (newas==NULL) {
		return ENOMEM;
	}

	/*
	 * Write this.
	 */

	(void)old;

	*ret = newas;
	return 0;
}

void
as_destroy(struct addrspace *as)
{
	/*
	 * Clean up as needed.
	 */

	kfree(as);
}

void
as_activate(void)
{
	struct addrspace *as;

	as = proc_getas();
	if (as == NULL) {
		/*
		 * Kernel thread without an address space; leave the
		 * prior address space in place.
		 */
		return;
	}

	/*
	 * Write this.
	 */
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
	/*
	 * Write this.
	 */

	(void)as;
	(void)vaddr;
	(void)memsize;
	(void)readable;
	(void)writeable;
	(void)executable;
	return ENOSYS;
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
	/*
	 * Write this.
	 */

	(void)as;

	/* Initial user-level stack pointer */
	*stackptr = USERSTACK;

	return 0;
}


