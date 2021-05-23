/*系统调用内核实现*/
#include "syscall-init.h"
#include "syscall.h"
#include "print.h"
#include "thread.h"
#include "console.h"
#include "string.h"
#include "memory.h"
#include "fs.h"
#include "fork.h"

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
  syscall_table[SYS_FORK]   = sys_fork;
  syscall_table[SYS_READ]   = sys_read;
  syscall_table[SYS_PUTCHAR]= sys_putchar;
  syscall_table[SYS_CLEAR]  = cls_screen;

  syscall_table[SYS_GETCWD] = sys_getcwd;
  syscall_table[SYS_OPEN]   = sys_open;
  syscall_table[SYS_CLOSE]  = sys_close;
  syscall_table[SYS_LSEEK]  = sys_lseek;
  syscall_table[SYS_UNLINK] = sys_unlink;
  syscall_table[SYS_MKDIR]  = sys_mkdir;
  syscall_table[SYS_OPENDIR]= sys_opendir;
  syscall_table[SYS_CLOSEDIR]=sys_closedir;
  syscall_table[SYS_CHDIR]  = sys_chdir;
  syscall_table[SYS_RMDIR]  = sys_rmdir;
  syscall_table[SYS_READDIR]= sys_readdir;
  syscall_table[SYS_REWINDDIR]=sys_rewinddir;
  syscall_table[SYS_STAT]   = sys_stat;
  syscall_table[SYS_PS]     = sys_ps;
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
 * 向屏幕上输出一段字符串
uint32_t sys_write(char *str)
{
  console_put_str(str);
  return strlen(str);
}
*/

/*
 * sys_malloc()和sys_free()实现在memory.c中
 * */
