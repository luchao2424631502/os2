#ifndef __USERPROG_SYSCALLINIT_H
#define __USERPROG_SYSCALLINIT_H

#include "stdint.h"

/*系统调用初始化*/
void syscall_init();

/*系统调用函数的内核实现*/
uint32_t sys_getpid();

#endif
