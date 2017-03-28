//Declaring an array of structs for the process table 
#include <limits.h>
#include <proc_table.h>

struct proc* proc_table[MAX_PROC];

//Initislize the proc count to 0
volatile int proc_count = 0;
struct lock *plock;

pid_t create_pid(void)
{
	int i;
	for(i=1 ; i<MAX_PROC && proc_table[i]!= NULL ; i++)
	{

	}
	if(i==1) {
		plock = lock_create("ptable");
	}
	if(i == MAX_PROC)
	{
		return -1;
	}	
	return (pid_t)i;	
}

void recycle_pid(pid_t pid)
{
	lock_acquire(plock);
	if(pid < MAX_PROC && pid >= 1)
	{
		proc_table[pid] = NULL;
		proc_count--;
	}
	lock_release(plock);
}

struct proc* get_proc(pid_t pid)
{
	lock_acquire(plock);
	
	if(pid < MAX_PROC && pid >= 1)
	{
		lock_release(plock);
		return proc_table[pid];
	}
	lock_release(plock);
	return NULL;
}
void add_proc(struct proc * p) 
{
	if(proc_count>0)
		lock_acquire(plock);
	proc_table[p->proc_id] = p;
	proc_count++;
	if(lock_do_i_hold(plock))	//in other words, when we're not adding the kernel proc
		lock_release(plock);
//	spinlock_release(splock);
}
