#include "print.h"
#include "init.h"
#include "debug.h"
#include "memory.h"
#include "interrupt.h"
#include "thread.h"
#include "console.h"
#include "keyboard.h"
#include "ioqueue.h"

#include "process.h"
#include "syscall.h"
#include "syscall-init.h"

void k_thread_a(void *);
void k_thread_b(void *);

void u_prog_a();
void u_prog_b();
int prog_a_pid = 0;
int prog_b_pid = 0;

int main()
{
  put_str("I'm kernel\n");
  init_all();

  /*用户进程*/
  process_execute(u_prog_a,"user_prog_a");
  process_execute(u_prog_b,"user_prog_b");

  /*接收时钟中断*/
  intr_enable();
  
  {
    console_put_str(" main_pid:0x");
    console_put_int(sys_getpid());
    console_put_char('\n');
  }

  /*需要通过kernel线程来打印用户进程Pid,因为用户进程没有权限使用console_put_xxx*/
  thread_start("k_thread_a",31,k_thread_a,"argA ");
  thread_start("k_thread_b",31,k_thread_b,"argB ");
  while(1){}
  return 0;
}

void k_thread_a(void *arg)
{
  char *para = arg;
  console_put_str(" thread_a_pid:0x");
  console_put_int(sys_getpid());
  console_put_char('\n');
  console_put_str(" prog_a_pid:0x");
  console_put_int(prog_a_pid);
  console_put_char('\n');
  while (1) {}
}

void k_thread_b(void *arg)
{
  char *para = arg;
  console_put_str(" thread_b_pid:0x");
  console_put_int(sys_getpid());
  console_put_char('\n');
  console_put_str(" prog_b_pid:0x");
  console_put_int(prog_b_pid);
  console_put_char('\n');
  while (1) {}
}

void u_prog_a()
{
  /*为什么ring 3程序调用不了sys_getpid()?
   * 因为当前选择子调用分页中的Kernel描述符权限不够
   * */
  prog_a_pid = getpid();
  while (1) {}
}

void u_prog_b()
{
  prog_b_pid = getpid();
  while (1) {}
}
