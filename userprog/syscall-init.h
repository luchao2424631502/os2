#ifndef __USERPROG_SYSCALLINIT_H
#define __USERPROG_SYSCALLINIT_H

#include "stdint.h"

/*系统调用初始化*/
void syscall_init();

/*系统调用函数的内核实现*/
uint32_t sys_getpid();

/*新版实现在fs.c中,声明在fs.h中
uint32_t sys_write();
*/

/* 声明在memory.h中,因为实现也在memory.c
void *sys_malloc(uint32_t);
void sys_free(void *);
*/

#endif
