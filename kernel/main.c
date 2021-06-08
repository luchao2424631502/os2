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
//<<<<<<< HEAD
//#include "string.h"
//=======
#include "syscall.h"
#include "syscall-init.h"
#include "stdio.h"
#include "stdio-kernel.h"
#include "super_block.h"
//>>>>>>> master

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

extern uint8_t _binary_graphics_1_in_start[];
extern uint8_t _binary_graphics_1_in_end[];
extern uint8_t _binary_graphics_1_in_size[];

int main()
{
  // put_str("I'm kernel\n");
  /*graphics模块现在位于内核主线程中,*/
  init_all();
  
  /*将裸盘hd60M.img中的 prog_no_arg 程序写入文件系统中*/

//<<<<<<< HEAD

  debug_print_int("start=",(uint32_t)_binary_graphics_1_in_start);
  debug_print_int("end=",(uint32_t)_binary_graphics_1_in_end);
  debug_print_int("size=",(uint32_t)_binary_graphics_1_in_size);

  uint8_t *point = (uint8_t *)0xc00091c0;
  debug_print_int("off_set_0x12=",(uint8_t)*(point+0x12));
  debug_print_int("off_set_0x13=",(uint8_t)*(point+0x13));
  put_int(99);
  put_char('\n');
  put_int(-12);

  /*
  thread_start("k_thread_a",31,k_thread_a,"argA ");
  thread_start("k_thread_b",31,k_thread_b,"argB ");
  process_execute(u_prog_a,"user_prog_a");
  process_execute(u_prog_b,"user_prog_b");
  */
//=======
  /*
  uint32_t file_size = 5653;
  uint32_t sec_cnt = DIV_ROUND_UP(file_size,512);
>>>>>>> master

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
  // console_put_str("[rabbit@localhost /]$ ");
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
