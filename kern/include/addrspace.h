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

#ifndef _ADDRSPACE_H_
#define _ADDRSPACE_H_
#define PAGE_FIXED 1
#define PAGE_FREE 2
#define PAGE_USER 3
#define PAGE_SWAPPING 4 
#define PAGE_COPYING 5

/*
 * Address space structure and operations.
 */

//will be followed by swapin basically means that the lock for this pte wont be release after a swapout (see as copy)
 #define WILL_BE_FOLLOWED_BY_SWAPIN 1
 #define WILL_NOT_BE_FOLLOWED_BY_SWAPIN 2


#include <vm.h>
#include "opt-dumbvm.h"
 #include <mips/tlb.h>
 #include <spl.h>

#include <spinlock.h>
#define CUSTOM_STACK_SIZE 1024*PAGE_SIZE //try experimenting with different sizes

#define PTE_ON_DISK 1
#define PTE_IN_MEMORY 2     //for 3.2 we'll have all pages in MEMORY
#define PTE_UNASSIGNED 3  //physical page is not assigned to such a virtual page

#define PTE_GREEN 0
#define PTE_GREY 1

struct vnode;


extern struct spinlock core_lock;
extern struct core_entry * coremap;
extern unsigned int used_bytes;
extern int total_pages;
extern struct vnode *swap_file;
//extern paddr_t first_free;



/*
 * Address space - data structure associated with the virtual memory
 * space of a process.
 *
 * You write this.
 */

 struct region {
    vaddr_t start;
    vaddr_t end;
    struct region* next;    
 };

 struct pte {
    vaddr_t vpn;
    paddr_t ppn;
    int state;
    off_t disk_offset;
    struct pte *next;
    struct lock* pte_lock;
    int destroy;
 };

struct core_entry {
 	unsigned int state;
 	int chunk_size;
	struct pte *pte_ptr;
    int color;
};
struct addrspace {
#if OPT_DUMBVM
        vaddr_t as_vbase1;
        paddr_t as_pbase1;
        size_t as_npages1;
        vaddr_t as_vbase2;
        paddr_t as_pbase2;
        size_t as_npages2;
        paddr_t as_stackpbase;
#else
        /* Put stuff here for your VM system */
        struct region *a_regions;
        vaddr_t total_region_end; //end of a_regions (does not include heap/stack!)
        struct region *stack_region;
        //Stack start is always 80mllion and sytack end is the current stack pointer(which changes with alloc/deallocations)
        struct region *heap_region;
        //page table
        struct pte *page_table;

#endif
};



paddr_t trim_physical(paddr_t original);
vaddr_t trim_virtual(vaddr_t original);

void vm_bootstrap(void);

/* Fault handling function called by trap code */
int vm_fault(int faulttype, vaddr_t faultaddress);

/* Allocate/free kernel heap pages (called by kmalloc/kfree) */
vaddr_t alloc_kpages(unsigned npages);
void free_kpages(vaddr_t addr);
paddr_t alloc_upage(struct pte *page_pte, int decision);
void free_upage(vaddr_t addr);
void delete_pte(struct addrspace *as, paddr_t addr);

/*
 * Return amount of memory (in bytes) used by allocated coremap pages.  If
 * there are ongoing allocations, this value could change after it is returned
 * to the caller. But it should have been correct at some point in time.
 */
unsigned int coremap_used_bytes(void);

/* TLB shootdown handling called from interprocessor_interrupt */
void vm_tlbshootdown(const struct tlbshootdown *);

/*
 * Functions in addrspace.c:
 *
 *    as_create - create a new empty address space. You need to make
 *                sure this gets called in all the right places. You
 *                may find you want to change the argument list. May
 *                return NULL on out-of-memory error.
 *
 *    as_copy   - create a new address space that is an exact copy of
 *                an old one. Probably calls as_create to get a new
 *                empty address space and fill it in, but that's up to
 *                you.
 *
 *    as_activate - make curproc's address space the one currently
 *                "seen" by the processor.
 *
 *    as_deactivate - unload curproc's address space so it isn't
 *                currently "seen" by the processor. This is used to
 *                avoid potentially "seeing" it while it's being
 *                destroyed.
 *
 *    as_destroy - dispose of an address space. You may need to change
 *                the way this works if implementing user-level threads.
 *
 *    as_define_region - set up a region of memory within the address
 *                space.
 *
 *    as_prepare_load - this is called before actually loading from an
 *                executable into the address space.
 *
 *    as_complete_load - this is called when loading from an executable
 *                is complete.
 *
 *    as_define_stack - set up the stack region in the address space.
 *                (Normally called *after* as_complete_load().) Hands
 *                back the initial stack pointer for the new process.
 *
 * Note that when using dumbvm, addrspace.c is not used and these
 * functions are found in dumbvm.c.
 */

struct addrspace *as_create(void);
int               as_copy(struct addrspace *src, struct addrspace **ret);
void              as_activate(void);
void              as_deactivate(void);
void              as_destroy(struct addrspace *);

int               as_define_region(struct addrspace *as,
                                   vaddr_t vaddr, size_t sz,
                                   int readable,
                                   int writeable,
                                   int executable);
int               as_prepare_load(struct addrspace *as);
int               as_complete_load(struct addrspace *as);
int               as_define_stack(struct addrspace *as, vaddr_t *initstackptr);


/*
 * Functions in loadelf.c
 *    load_elf - load an ELF user program executable into the current
 *               address space. Returns the entry point (initial PC)
 *               in the space pointed to by ENTRYPOINT.
 */

int load_elf(struct vnode *v, vaddr_t *entrypoint);


#endif /* _ADDRSPACE_H_ */
