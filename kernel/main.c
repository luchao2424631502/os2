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
  // process_execute(u_prog_a,"user_prog_a");
  // process_execute(u_prog_b,"user_prog_b");

  /*需要通过kernel线程来打印用户进程Pid,因为用户进程没有权限使用console_put_xxx*/
  // thread_start("k_thread_a",31,k_thread_a,"argA ");
  // thread_start("k_thread_b",31,k_thread_b,"argB ");

  printf("/dir1 content before delete /dir1/subdir1:\n");
  struct dir *dir = sys_opendir("/dir1/");
  char *type = NULL;
  struct dir_entry *dir_e = NULL;
  while ((dir_e = sys_readdir(dir)))
  {
    if (dir_e->f_type == FT_REGULAR)
    {
      type = "regular";
    }
    else 
    {
      type = "directory";
    }
    printf("    %s    %s\n,",type,dir_e->filename);
  }
  //上面是遍历/dir1/目录

  //删除非空目录
  printf("try to delete nonempty directory /dir1/subdir1\n"); 
  if (sys_rmdir("/dir1/subdir1") == -1)
  {
    printf("sys_rmdir: /dir1/subdir1 delete fail!\n");
  }

  //rmdir()删除文件
  printf("try to delete /dir1/subdir1/file2\n");
  if (sys_rmdir("/dir1/subdir1/file2") == -1)
  {
    printf("sys_rmdir: /dir1/subdir1/file2 delete fail!\n"); 
  }

  //unlink()删除文件,肯定是成功的
  if (sys_unlink("/dir1/subdir1/file2") == 0)
  {
    printf("sys_unlink: /dir1/subdir1/file2 delete done\n");
  }

  printf("try to delete directory /dir1/subdir1 again\n");
  if (sys_rmdir("/dir1/subdir1") == 0)
  {
    printf("/dir1/subdir1 delete done!\n");
  }

  printf("/dir1 content after delete /dir1/subdir1:\n");
  sys_rewinddir(dir);
  while ((dir_e = sys_readdir(dir)))
  {
    if (dir_e->f_type == FT_REGULAR)
      type = "regular";
    else 
      type = "directory";
    printf("    %s    %s\n,",type,dir_e->filename);
  }

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
