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
#include "stdio.h"

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

  /*接收时钟中断*/
  intr_enable();

  /*用户进程*/
  process_execute(u_prog_a,"user_prog_a");
  process_execute(u_prog_b,"user_prog_b");

  /*需要通过kernel线程来打印用户进程Pid,因为用户进程没有权限使用console_put_xxx*/
  thread_start("k_thread_a",31,k_thread_a,"argA ");
  thread_start("k_thread_b",31,k_thread_b,"argB ");

  while(1){}
  return 0;
}

//测试sys_free()
void k_thread_a(void *arg)
{
  void *addr1 = sys_malloc(256);
  void *addr2 = sys_malloc(255);
  void *addr3 = sys_malloc(254);
  console_put_str(" thread_a malloc addr:0x");
  console_put_int((int)addr1);
  console_put_str(",");
  console_put_int((int)addr2);
  console_put_char(',');
  console_put_int((int)addr3);
  console_put_char('\n');

  int cpu_delay = 100000;
  while (cpu_delay-- > 0)
  {}

  sys_free(addr1);
  sys_free(addr2);
  sys_free(addr3);

  while (1) {}
}

void k_thread_b(void *arg)
{
  void *addr1 = sys_malloc(256);
  void *addr2 = sys_malloc(255);
  void *addr3 = sys_malloc(254);
  console_put_str(" thread_b malloc addr:0x");
  console_put_int((int)addr1);
  console_put_str(",");
  console_put_int((int)addr2);
  console_put_char(',');
  console_put_int((int)addr3);
  console_put_char('\n');

  int cpu_delay = 100000;
  while (cpu_delay-- > 0)
  {}

  sys_free(addr1);
  sys_free(addr2);
  sys_free(addr3);

  while (1) {}
}

void u_prog_a()
{
  /*为什么ring 3程序调用不了sys_getpid()?
   * 因为当前选择子调用分页中的Kernel描述符权限不够
   * */
  // printf(" printf_prog_a_pid:0x%x\n",getpid());

  void *addr1 = malloc(256);
  void *addr2 = malloc(255);
  void *addr3 = malloc(254);
  printf(" prog_a malloc addr:0x%x,0x%x,0x%x\n",(int)addr1,(int)addr2,(int)addr3);

  int cpu_delay = 100000;
  while (cpu_delay-- > 0)
  {}
  free(addr1);
  free(addr2);
  free(addr3);

  while (1) {}
}

void u_prog_b()
{
  void *addr1 = malloc(256);
  void *addr2 = malloc(255);
  void *addr3 = malloc(254);
  printf(" prog_b malloc addr:0x%x,0x%x,0x%x\n",(int)addr1,(int)addr2,(int)addr3);

  int cpu_delay = 100000;
  while (cpu_delay-- > 0)
  {}
  free(addr1);
  free(addr2);
  free(addr3);

  while (1) {}
}
