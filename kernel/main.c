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

/*用户进程*/
// process_execute(u_prog_a,"user_prog_a");
// process_execute(u_prog_b,"user_prog_b");

/*需要通过kernel线程来打印用户进程Pid,因为用户进程没有权限使用console_put_xxx*/
// thread_start("k_thread_a",31,k_thread_a,"argA ");
// thread_start("k_thread_b",31,k_thread_b,"argB ");

int main()
{
  put_str("I'm kernel\n");
  init_all();
  
  // int res = sys_unlink("/prog_no_arg");
  // if (res == -1)
  // {
    // printk("unlink error\n");
  // }

  /*将裸盘hd60M.img中的 prog_no_arg 程序写入文件系统中*/
  uint32_t file_size = 4488;
  uint32_t sec_cnt = DIV_ROUND_UP(file_size,512);

  struct disk *sda = &channels[0].devices[0];
  void *prog_buf = sys_malloc(file_size);
  ide_read(sda,300,prog_buf,sec_cnt);

  int32_t fd = sys_open("/prog_no_arg",O_CREAT|O_RDWR);
  if (fd != -1)
  {
    if (sys_write(fd,prog_buf,file_size) == -1)
    {
      printk("file write error\n");
      while (1);
    }
  }
  /*----*/

  cls_screen();

  while(1){}
  return 0;
}

void init()
{
  uint32_t ret_pid = fork();
  if (ret_pid)//父进程
  {
    while (1);
  }
  else //子进程
  {
    my_shell();
  }
  panic("init: should not be here");
}
