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
  /*graphics模块现在位于内核主线程中,*/
  init_all();
  
  /*将裸盘hd60M.img中的 prog_no_arg 程序写入文件系统中*/

//<<<<<<< HEAD

  //80*25模式下的
  // debug_print_int("start=",(uint32_t)_binary_graphics_1_in_start);

  int mouse_phase = 0;
  // unsigned char mouse_dbuf[3];
  uint8_t mouse_dbuf[3];
  uint8_t data;

  while (1)
  {
    enum intr_status old_status = intr_disable();
    if (!ioq_empty(&mouse_buf))
      data = ioq_getchar(&mouse_buf);
    intr_set_status(old_status);

    if (mouse_phase == 0)
    {
      //等待进入鼠标的0xfa状态
      if (data == 250)
      {
        mouse_phase = 1;
      }
    }
    //拿到第一个字节
    else if (mouse_phase == 1)
    {
      mouse_dbuf[0] = data;
      mouse_phase = 2;
    }
    //拿到第2个字节
    else if (mouse_phase == 2)
    {
      mouse_dbuf[1] = data;
      mouse_phase = 3;
    }
    //拿到第3个字节
    else if (mouse_phase == 3) 
    {
      mouse_dbuf[2] = data;
      mouse_phase = 1;

      char buf[50];
      sprintf(buf,"%d %d %d",mouse_dbuf[0],mouse_dbuf[1],mouse_dbuf[2]);
      putfont8_str((uint8_t *)0xc00a0000,320,0,32,0,buf);
      boxfill8((uint8_t *)0xc00a0000,320,7,0,32,0+88,32+32);

      memset(buf,0,50);
      // putfont8_int((uint8_t*)0xc00a0000,320,0,48,0,mouse_dbuf[0]);
      // putfont8_int((uint8_t*)0xc00a0000,320,0+32,48,0,mouse_dbuf[1]);
      // putfont8_int((uint8_t*)0xc00a0000,320,0+64,48,0,mouse_dbuf[2]);
    }
  }

  
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
