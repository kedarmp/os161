#include<kern/unistd.h>
#include<types.h>
//#include<copyinout.h>
#include<lib.h>

ssize_t sys_read(uint32_t fd_u, userptr_t buffer_u, uint32_t size_u);
