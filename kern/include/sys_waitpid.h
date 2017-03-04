#include <proc.h>

pid_t
sys_waitpid(pid_t pid, userptr_t status, int options,int *errptr);
