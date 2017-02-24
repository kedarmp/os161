#include<kern/unistd.h>
#include<types.h>
#include<copyinout.h>
#include<lib.h>

ssize_t sys_getcwd(userptr_t buff_u, size_t bufflen, int * err);
