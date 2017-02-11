/*
 * Copyright (c) 2001, 2002, 2009
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

/*
 * Driver code is in kern/tests/synchprobs.c We will
 * replace that file. This file is yours to modify as you see fit.
 *
 * You should implement your solution to the whalemating problem below.
 */

#include <types.h>
#include <lib.h>
#include <thread.h>
#include <test.h>
#include <synch.h>

/*
 * Called by the driver during initialization.
 */

struct semaphore *malesem;
struct semaphore *femalesem;
struct semaphore *matchsem;
//struct spinlock wh_splock;
	
void whalemating_init() {
	malesem = sem_create("male",0);	
	femalesem = sem_create("female",0);
	matchsem = sem_create("match",1);
	//spinlock_init(&wh_splock);
	return;
}

/*
 * Called by the driver during teardown.
 */

void
whalemating_cleanup() {
	sem_destroy(malesem);
	sem_destroy(femalesem);
	sem_destroy(matchsem);
	//spinlock_cleanup(&wh_splock);
	return;
}

void
male(uint32_t index)
{
	kprintf("male(%d)called ",index);
	//spinlock_acquire(&wh_splock);
	male_start(index);
	P(malesem);
	//male_start(index);	
	male_end(index);
	//spinlock_release(&wh_splock);
	return;
}

void
female(uint32_t index)
{
	kprintf(" female(%d)called ",index);
	//spinlock_acquire(&wh_splock);
	female_start(index);
	P(femalesem);
	//female_start(index);
	female_end(index);
	//spinlock_release(&wh_splock);
	return;
}

void
matchmaker(uint32_t index)
{
	kprintf(" matchmaker(%d)called ",index);
	//spinlock_acquire(&wh_splock);
	matchmaker_start(index);
	V(malesem);
	V(femalesem);
	matchmaker_end(index);
	V(matchsem);
	//spinlock_release(&wh_splock);	
}
