//Declaring an array of structs for the process table 
#include <limits.h>
#include <proc_table.h>

struct proc* proc_table[PID_MAX];

//Initislize the proc count to 0
volatile int proc_count = 0;


pid_t create_pid(void)
{
	int i;
	for(i=1 ; i<PID_MAX && proc_table[i]!= NULL ; i++)
	{

	}
	if(i == PID_MAX)
	{
		return -1;
	}	
	return (pid_t)i;	
}

void recycle_pid(pid_t pid)
{
	if(pid < PID_MAX && pid >= 1)
	{
		proc_table[pid] = NULL;
	}
}

struct proc* get_proc(pid_t pid)
{
	if(pid < PID_MAX && pid >= 1)
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
