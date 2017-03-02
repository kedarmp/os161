//Declaring an array of structs for the process table 
#include <proc_table.h>
#include <limits.h>

struct proc* proc_table[PID_MAX];

//Initislize the proc count to 0
int proc_count = 0;

//Initialize the lock
struct lock *proc_lock;

//Variable to keep track of init funciton calling
int initialize_count = 0;

void init_proc_table(void)
{
	if(initialize_count != 0)
	{
		return;
	}
	initialize_count = 1;
	proc_lock = lock_create("proclock");
}

pid_t create_pid(void)
{
	int i = 1;
	for( ; i<PID_MAX && proc_table[i]!= NULL ; i++)
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
	lock_acquire(proc_lock);
	if(pid < PID_MAX && pid >= 1)
	{
		proc_table[pid] = NULL;
	}
	lock_release(proc_lock);
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
	lock_acquire(proc_lock);
	proc_table[p->proc_id] = p;
	proc_count++;
	lock_release(proc_lock);
}