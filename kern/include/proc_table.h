#include <proc.h>
#include <types.h>
#include <synch.h>
//#include <unistd.h>

#define MAX_PROC 100
//Generate PID
pid_t create_pid(void);

//Recycle PID
void recycle_pid(pid_t pid);

//Get the process
struct proc* get_proc(pid_t pid);
void add_proc(struct proc * p);

//Number of processes
extern volatile int proc_count;

