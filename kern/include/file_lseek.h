#include <kern/unistd.h>
#include <types.h>
#include <copyinout.h>
#include <lib.h>

off_t sys_lseek(int fd, uint32_t high, uint32_t low, int *whence, int *errptr);