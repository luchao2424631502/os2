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
#include "stdio-kernel.h"
#include "super_block.h"

#include "file.h"
#include "fs.h"
#include "dir.h"
#include "shell.h"
#include "assert.h"

#include "graphics.h"
#include "font.h"
#include "vramio.h"
#include "mouse.h"
#include "paint.h"

/*用户进程*/
// process_execute(u_prog_a,"user_prog_a");
// process_execute(u_prog_b,"user_prog_b");

/*需要通过kernel线程来打印用户进程Pid,因为用户进程没有权限使用console_put_xxx*/
// thread_start("k_thread_a",31,k_thread_a,"argA ");
// thread_start("k_thread_b",31,k_thread_b,"argB ");

extern uint8_t _binary_graphics_1_in_start[];
extern uint8_t _binary_graphics_1_in_end[];
extern uint8_t _binary_graphics_1_in_size[];

int main()
{
  /*graphics模块现在位于内核主线程中
    loader->main,在开头初始化所有需要准备的. 
   * */
  init_all();
  console_put_str("[rabbit@localhost /]$ ");
  
  //6-8:添加 graphics 的内核线程来测试图形操作
  thread_start("k_graphics",31,kernel_graphics,NULL);
  thread_start("k_g_mouse",31,k_mouse,NULL);

  /*
  uint32_t file_size = 5653;
  uint32_t sec_cnt = DIV_ROUND_UP(file_size,512);

  struct disk *sda = &channels[0].devices[0];
  void *prog_buf = sys_malloc(file_size);
  ide_read(sda,300,prog_buf,sec_cnt);

  int32_t fd = sys_open("/cat",O_CREAT|O_RDWR);
  if (fd != -1)
  {
    if (sys_write(fd,prog_buf,file_size) == -1)
    {
      printk("file write error\n");
      while (1);
    }
  }
  */

  // cls_screen();
  thread_exit(running_thread(),true);
  return 0;
}

/*init进程不断的回收孤儿进程(僵尸进程)*/
void init()
{
  uint32_t ret_pid = fork();
  if (ret_pid)//父进程
  {
    int status;
    int child_pid;
    while (1)
    {
      child_pid = wait(&status);
      printf("[init process],My Pid is 1,I receive a child,It's pid is %d,status is %d\n",child_pid,status);
    }
  }
  else //子进程
  {
    my_shell();
  }
  panic("init: should not be here");
}
