#include <sys_getpid.h>
#include <current.h>
#include <proc.h>

pid_t
sys_getpid(void) {
	return curproc->proc_id;
}