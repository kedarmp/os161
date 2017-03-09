//Declaring an array of structs for the process table 
#include <limits.h>
#include <proc_table.h>
#define MAX_PROC 100

struct proc* proc_table[MAX_PROC];

//Initislize the proc count to 0
volatile int proc_count = 0;


pid_t create_pid(void)
{
	int i;
	for(i=1 ; i<MAX_PROC && proc_table[i]!= NULL ; i++)
	{

	}
	if(i == MAX_PROC)
	{
		return -1;
	}	
	return (pid_t)i;	
}

void recycle_pid(pid_t pid)
{
	if(pid < MAX_PROC && pid >= 1)
	{
		proc_table[pid] = NULL;
	}
}

struct proc* get_proc(pid_t pid)
{
	if(pid < MAX_PROC && pid >= 1)
	{
		return proc_table[pid];
	}
	return NULL;
}
void add_proc(struct proc * p) 
{
//	spinlock_acquire(splock);
	proc_table[p->proc_id] = p;
	proc_count++;
//	spinlock_release(splock);
}
