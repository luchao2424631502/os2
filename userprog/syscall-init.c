/*系统调用内核实现*/
#include "syscall-init.h"
#include "syscall.h"
#include "print.h"
#include "thread.h"
#include "console.h"
#include "string.h"
#include "memory.h"

/*内核提供调用的函数个数*/
#define SYSCALL_NR 32
typedef void* syscall;
/*系统调用的内核入口*/
syscall syscall_table[SYSCALL_NR];


/*初始化系统调用,
 * 也就是向syscall_table注册内核调用的函数地址
 * */
void syscall_init()
{
  put_str("[syscall_init] start\n");
  syscall_table[SYS_GETPID] = sys_getpid; 
  syscall_table[SYS_WRITE]  = sys_write;
  syscall_table[SYS_MALLOC] = sys_malloc;
  syscall_table[SYS_FREE]   = sys_free;
  put_str("[syscall_init] done\n");
}

/*
 * API_0:返回当前进程的Pid
 * */
uint32_t sys_getpid()
{
  return running_thread()->pid; 
}

/*API_1:简单版write 
 * 向屏幕上输出一段字符串*/
uint32_t sys_write(char *str)
{
  console_put_str(str);
  return strlen(str);
}

/*
 * sys_malloc()和sys_free()实现在memory.c中
 * */
