#include <proc.h>
#define TYPE_EXITED 1
#define TYPE_RECEIVED_SIG 2

void sys_exit(int exitcode,int type);
