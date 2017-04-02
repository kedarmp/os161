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

/*
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 */

paddr_t coremap;
paddr_t first_free;
int used_bytes;

void vm_bootstrap(void) {

}

/* Fault handling function called by trap code */

void init_coremap() {
	int i=0;
	coremap = ram_getfirstfree();
	int ram_size = ram_getsize() - 0x0;
	//setup structs of core_entry and store them to RAM
	int total_pages = ram_size/PAGE_SIZE;
	first_free = coremap + npages * sizeof(struct core_entry);

	paddr_t traverse = coremap;
	//set up first few entries in coremap as fixed (these correspond to the initial kernel entries already present)
	int existing_pages_used = (coremap - 0x0)/PAGE_SIZE;
	for(i=0;i<existing_pages_used;i++) {
		struct core_entry e;
		e.state = PAGE_FIXED;
		e.chunk_size = 0;
		//put struct in physical mem
		*traverse = e;
		traverse += sizeof(struct core_entry);
	}


	//setup rest of pages
	for(;i<total_pages; i++) {
		struct core_entry e;
		e.state = PAGE_FREE;
		e.chunk_size = 0;
		//put struct in physical mem
		*traverse = e;
		traverse += sizeof(struct core_entry);
	}
	used_bytes = first_free - 0x0; //assume that we include existing kernel pages + coremap size also as "used"
}

int vm_fault(int faulttype, vaddr_t faultaddress) {
(void)faulttype;
(void)faultaddress;
return 0;
}

/* Allocate/free kernel heap pages (called by kmalloc/kfree) */
vaddr_t alloc_kpages(unsigned npages) {
		paddr_t traverse_end = coremap;
		paddr_t marker = traverse_end;
		struct core_entry e = *traverse_end;
		int found = 0;
		while(traverse_end<first_free) {
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
			traverse_end += sizeof(core_entry);
			e = *traverse_end;
			if(found==0) {	//we will start fresh. Marker should always point to the first struct of our desired "contiguous segment"
				marker = traverse_end;
			}
		}
		if(found == npages) {	//found "npages" free pages starting at traverse
			//paddr_t ret_address = traverse_end - (npages*sizeof(struct core_entry));

			//the segment from marker upto npages ahead should now be marked as fixed
			e = *marker;
			e.state = PAGE_FIXED;
			e.chunk_size = npages;
			for(int i=0; i<npages; i++) {
				e = *(marker + i*sizeof(struct core_entry));
				e.state = PAGE_FIXED;
				e.chunk_size = 0;
			}
			used_bytes += npages*PAGE_SIZE;
			return PADDR_TO_KVADDR(marker);
		}
		
	
	return NULL;

}
void free_kpages(vaddr_t addr) {
(void) addr;
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
void vm_tlbshootdown(const struct tlbshootdown *) {
	(void)tlbshootdown;
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


