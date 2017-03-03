#ifndef _SYSFORK_H_
#define _SYSFORK_H_

#include <mips/trapframe.h>
#include <proc.h>


pid_t sys_fork(struct trapframe* old_trapframe,struct proc* parent_proc,int *errptr);

#endif