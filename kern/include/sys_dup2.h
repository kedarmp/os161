#include <types.h>
#include <syscall.h>
#include <fhandle.h>
#include <proc.h>
#include <current.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <kern/limits.h>
#include <file_close.h>

int sys_dup2(int oldfd, int newfd, int *errptr);