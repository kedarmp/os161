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

/*
 * Synchronization primitives.
 * The specifications of the functions are in synch.h.
 */

#include <types.h>
#include <lib.h>
#include <spinlock.h>
#include <wchan.h>
#include <thread.h>
#include <current.h>
#include <synch.h>

////////////////////////////////////////////////////////////
//
// Semaphore.

struct semaphore *
sem_create(const char *name, unsigned initial_count)
{
	struct semaphore *sem;

	sem = kmalloc(sizeof(*sem));
	if (sem == NULL) {
		return NULL;
	}

	sem->sem_name = kstrdup(name);
	if (sem->sem_name == NULL) {
		kfree(sem);
		return NULL;
	}

	sem->sem_wchan = wchan_create(sem->sem_name);
	if (sem->sem_wchan == NULL) {
		kfree(sem->sem_name);
		kfree(sem);
		return NULL;
	}

	spinlock_init(&sem->sem_lock);
	sem->sem_count = initial_count;

	return sem;
}

void
sem_destroy(struct semaphore *sem)
{
	KASSERT(sem != NULL);

	/* wchan_cleanup will assert if anyone's waiting on it */
	spinlock_cleanup(&sem->sem_lock);
	wchan_destroy(sem->sem_wchan);
	kfree(sem->sem_name);
	kfree(sem);
}

void
P(struct semaphore *sem)
{
	KASSERT(sem != NULL);

	/*
	 * May not block in an interrupt handler.
	 *
	 * For robustness, always check, even if we can actually
	 * complete the P without blocking.
	 */
	KASSERT(curthread->t_in_interrupt == false);

	/* Use the semaphore spinlock to protect the wchan as well. */
	spinlock_acquire(&sem->sem_lock);
	while (sem->sem_count == 0) {
		/*
		 *
		 * Note that we don't maintain strict FIFO ordering of
		 * threads going through the semaphore; that is, we
		 * might "get" it on the first try even if other
		 * threads are waiting. Apparently according to some
		 * textbooks semaphores must for some reason have
		 * strict ordering. Too bad. :-)
		 *
		 * Exercise: how would you implement strict FIFO
		 * ordering?
		 */
		wchan_sleep(sem->sem_wchan, &sem->sem_lock);
	}
	KASSERT(sem->sem_count > 0);
	sem->sem_count--;
	spinlock_release(&sem->sem_lock);
}

void
V(struct semaphore *sem)
{
	KASSERT(sem != NULL);

	spinlock_acquire(&sem->sem_lock);

	sem->sem_count++;
	KASSERT(sem->sem_count > 0);
	wchan_wakeone(sem->sem_wchan, &sem->sem_lock);

	spinlock_release(&sem->sem_lock);
}

////////////////////////////////////////////////////////////
//
// Lock.

struct lock *
lock_create(const char *name)
{
	struct lock *lock;

	lock = kmalloc(sizeof(*lock));
	if (lock == NULL) {
		return NULL;
	}

	// add stuff here as needed

	lock->lk_name = kstrdup(name);
	if (lock->lk_name == NULL) {
		kfree(lock);
		return NULL;
	}

	lock->lock_wchan = wchan_create(lock->lk_name);
	if(lock->lock_wchan == NULL) {
		kfree(lock->lk_name);
		kfree(lock);
		return NULL;
	}
	lock->lock_thread = NULL;
	spinlock_init(&lock->lock_spinlock);
	lock->lock_count = 1;
	HANGMAN_LOCKABLEINIT(&lock->lk_hangman, lock->lk_name);


	return lock;
}

void
lock_destroy(struct lock *lock)
{
	KASSERT(lock != NULL);

	// add stuff here as needed
	KASSERT(lock->lock_thread == NULL);
	spinlock_cleanup(&lock->lock_spinlock);
	wchan_destroy(lock->lock_wchan);
	kfree(lock->lk_name);
	kfree(lock);
}

void
lock_acquire(struct lock *lock)
{

	KASSERT(lock != NULL);
	KASSERT(curthread->t_in_interrupt == false);
	//added on 2/7: if we already have a lock, no need to do anything
	if(lock_do_i_hold(lock))
	{
		return;
	}
	spinlock_acquire(&lock->lock_spinlock);
	while(lock->lock_count == 0) {
		wchan_sleep(lock->lock_wchan, &lock->lock_spinlock);
	}
	KASSERT(lock->lock_count > 0);
	lock->lock_count = 0;
	lock->lock_thread = curthread;
	spinlock_release(&lock->lock_spinlock);
	/* Call this (atomically) before waiting for a lock */
	//HANGMAN_WAIT(&curthread->t_hangman, &lock->lk_hangman);

	// Write this


	/* Call this (atomically) once the lock is acquired */

}

void
lock_release(struct lock *lock)
{
	/* Call this (atomically) when the lock is released */
	//HANGMAN_RELEASE(&curthread->t_hangman, &lock->lk_hangman);

	// Write this

	KASSERT(lock != NULL);
	//if we dont hold this lock, no point proceeding
	KASSERT(lock_do_i_hold(lock));//moved this line ABOVE spinlock acq 2/7
	spinlock_acquire(&lock->lock_spinlock);
	lock->lock_count = 1;
	lock->lock_thread = NULL;
	wchan_wakeone(lock->lock_wchan, &lock->lock_spinlock);
	spinlock_release(&lock->lock_spinlock);
}

bool
lock_do_i_hold(struct lock *lock)
{
	KASSERT(lock!=NULL);
	return (curthread == lock->lock_thread);
}

////////////////////////////////////////////////////////////
//
// CV


struct cv *
cv_create(const char *name)
{
	struct cv *cv;

	cv = kmalloc(sizeof(*cv));
	if (cv == NULL) {
		return NULL;
	}

	cv->cv_name = kstrdup(name);
	if (cv->cv_name==NULL) {
		kfree(cv);
		return NULL;
	}


	// add stuff here as needed
	cv->cv_wc = wchan_create(name);
	if(cv->cv_wc == NULL) {
		kfree(cv->cv_name);
		kfree(cv);
	}
	spinlock_init(&cv->cv_spinlock);
	return cv;
}

void
cv_destroy(struct cv *cv)
{
	KASSERT(cv != NULL);

	// add stuff here as needed

	kfree(cv->cv_name);
	spinlock_cleanup(&cv->cv_spinlock);
	wchan_destroy(cv->cv_wc);
	kfree(cv);
}

void
cv_wait(struct cv *cv, struct lock *lock)
{
	KASSERT(lock!=NULL);
	KASSERT(lock_do_i_hold(lock));
	spinlock_acquire(&cv->cv_spinlock);
	lock_release(lock);
	wchan_sleep(cv->cv_wc,&cv->cv_spinlock);
	spinlock_release(&cv->cv_spinlock);
	lock_acquire(lock);

}

void
cv_signal(struct cv *cv, struct lock *lock)
{
	KASSERT(lock!=NULL);
	KASSERT(lock_do_i_hold(lock));
	spinlock_acquire(&cv->cv_spinlock);
	wchan_wakeone(cv->cv_wc,&cv->cv_spinlock);
	spinlock_release(&cv->cv_spinlock);
}

void
cv_broadcast(struct cv *cv, struct lock *lock)
{
	KASSERT(lock!=NULL);
	KASSERT(lock_do_i_hold(lock));
	spinlock_acquire(&cv->cv_spinlock);
	wchan_wakeall(cv->cv_wc,&cv->cv_spinlock);	
	spinlock_release(&cv->cv_spinlock);

}


//////////////////////////////////////////////////////////
// Reader Writer 

unsigned int  MAX_READER_BACKLOG = 5;
unsigned int  WRITER_THRESHOLD = 5;

struct rwlock *
rwlock_create(const char *name)
{
	struct rwlock *rwlock;
	rwlock = kmalloc(sizeof(*rwlock));
	if(rwlock == NULL)
		return NULL;
	rwlock->rwlock_name = kstrdup(name);
	if(rwlock->rwlock_name == NULL) {
		kfree(rwlock);
		return NULL;
	}
	rwlock->block = sem_create("block", 0);
	if(rwlock->block == NULL) {
		kfree(rwlock->rwlock_name);
		kfree(rwlock);
		return NULL;
	}
	rwlock->ex = sem_create("ex", 1);
	if(rwlock->ex == NULL) {
		kfree(rwlock->rwlock_name);
		kfree(rwlock);
		sem_destroy(rwlock->block);
		return NULL;
	}

	rwlock->splock = lock_create("mylock");
	if(rwlock->splock == NULL) {
		kfree(rwlock->rwlock_name);
		kfree(rwlock);
		sem_destroy(rwlock->block);
		sem_destroy(rwlock->ex);
		return NULL;
	}
	rwlock->reader_count = rwlock->pending_r = rwlock->pending_w = rwlock->writer = 0;
	return rwlock;
}


void
rwlock_destroy(struct rwlock *rwlock)
{
	KASSERT(rwlock!=NULL);
	KASSERT(rwlock->reader_count == 0);
	KASSERT(rwlock->pending_r == 0);
	KASSERT(rwlock->pending_w == 0);
	KASSERT(rwlock->writer == 0);
	kfree(rwlock->rwlock_name);
	lock_destroy(rwlock->splock);
	sem_destroy(rwlock->ex);
	sem_destroy(rwlock->block);
}

void rwlock_acquire_read(struct rwlock *rwlock) {
//	extern int WRITER_THRESHOLD;
	KASSERT(rwlock!=NULL);
	lock_acquire(rwlock->splock);
	rwlock->pending_r = rwlock->pending_r + 1;
			
	if(rwlock->writer == 1 || rwlock->pending_w>WRITER_THRESHOLD) {	
	//one writer is writing currently OR there is at least one writer waiting to write
		lock_release(rwlock->splock);
		P(rwlock->block);
	}
	//if we're the first reader(post block or otherwise), wait for (possible) writer to leave(ex)
	lock_acquire(rwlock->splock);
	if(rwlock->reader_count==0) {
		lock_release(rwlock->splock);
		P(rwlock->ex);
		lock_acquire(rwlock->splock);	//reacquire lock before coming out of condition
	}
	
	//there are no writers actively writing
	rwlock->pending_r = rwlock->pending_r - 1;
	rwlock->reader_count = rwlock->reader_count + 1;
	lock_release(rwlock->splock);
}

void rwlock_release_read(struct rwlock *rwlock)
{
	KASSERT(rwlock!=NULL);
	
	lock_acquire(rwlock->splock);
	KASSERT(rwlock->reader_count > 0);
	rwlock->reader_count = rwlock->reader_count - 1;
	if(rwlock->reader_count==0)	//free ex. If there's a writer waiting, it can go ahead
		V(rwlock->ex);
	lock_release(rwlock->splock);
}

void rwlock_acquire_write(struct rwlock *rwlock)
{
	KASSERT(rwlock!=NULL);
	//increment pending writers
	lock_acquire(rwlock->splock);
	rwlock->pending_w = rwlock->pending_w + 1;
	lock_release(rwlock->splock);
	//wait for all readers to leave
	P(rwlock->ex);
	//decrement pending writers
	lock_acquire(rwlock->splock);
	rwlock->pending_w = rwlock->pending_w - 1;
	//set writer = 1; means there is one live writer(used by readers)
	rwlock->writer = 1;
	lock_release(rwlock->splock);
	
}

void rwlock_release_write(struct rwlock *rwlock)
{
	KASSERT(rwlock!=NULL);
	unsigned int i=0;
	lock_acquire(rwlock->splock);
	KASSERT(rwlock->writer == 1);
	rwlock->writer = 0;
	if(rwlock->pending_w==0) {
		for(i=0;i<rwlock->pending_r;i++)
			V(rwlock->block);
	}

	else if(rwlock->pending_r >  MAX_READER_BACKLOG)	{ //release extra threads for reading
		for(i=0;i<(rwlock->pending_r - MAX_READER_BACKLOG);i++)
			V(rwlock->block);
	}
	lock_release(rwlock->splock);
	V(rwlock->ex);
}

