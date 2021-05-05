#ifndef __LIB_USER_SYSCALL_H
#define __LIB_USER_SYSCALL_H

#include "stdint.h"

/*根据内核将系统调用的实现顺序
 *来在用户空间定义api的index顺序*/
enum SYSCALL_NR{
  SYS_GETPID,
  SYS_WRITE,
  SYS_MALLOC,
  SYS_FREE
};

/*用户空间api的函数声明*/
uint32_t getpid(void);

uint32_t write(char *str);

void *malloc(uint32_t);

void free(void *);

#endif
