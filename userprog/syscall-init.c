/*系统调用内核实现*/
#include "syscall-init.h"
#include "syscall.h"
#include "print.h"
#include "thread.h"


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
  put_str("[syscall_init] done\n");
}

/*
 * API_0:返回当前进程的Pid
 * */
uint32_t sys_getpid()
{
  return running_thread()->pid; 
}
