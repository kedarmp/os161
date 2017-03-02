#include <proc.h>
#include <types.h>
#include <synch.h>
//#include <unistd.h>

//Generate PID
pid_t create_pid(void);

//Recycle PID
void recycle_pid(pid_t pid);

//Get the process
struct proc* get_proc(pid_t pid);
void add_proc(struct proc * p);

//Number of processes
extern int proc_count;

//Lock for the process
extern struct lock *proc_lock;

//Initialize the proc table
void init_proc_table(void);