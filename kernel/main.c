#include "main.h"
#include "print.h"
#include "init.h"
#include "debug.h"
#include "memory.h"
#include "interrupt.h"
#include "thread.h"
#include "console.h"
#include "keyboard.h"
#include "ioqueue.h"
#include "string.h"

#include "process.h"
#include "syscall.h"
#include "syscall-init.h"
#include "stdio.h"
#include "super_block.h"

#include "file.h"
#include "fs.h"
#include "dir.h"


int main()
{
  put_str("I'm kernel\n");
  init_all();

  /*用户进程*/
  // process_execute(u_prog_a,"user_prog_a");
  // process_execute(u_prog_b,"user_prog_b");

  /*需要通过kernel线程来打印用户进程Pid,因为用户进程没有权限使用console_put_xxx*/
  // thread_start("k_thread_a",31,k_thread_a,"argA ");
  // thread_start("k_thread_b",31,k_thread_b,"argB ");

  while(1){}
  return 0;
}

void init()
{
  uint32_t ret_pid = fork();
  if (ret_pid)
  {
    printf("father,pid is %d,child pid is %d\n",getpid(),ret_pid);
  }
  else 
  {
    printf("child,pid is %d,ret pid is %d\n",getpid(),ret_pid);
  }
  while(1);
}
