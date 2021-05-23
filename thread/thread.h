#ifndef __THREAD_THREAD_H
#define __THREAD_THREAD_H

#include "stdint.h"
#include "list.h" //数据结构用来调度
#include "bitmap.h"
#include "memory.h"

#define MAX_FILES_OPEN_PER_PROC 8

/*定义了一个函数类型,内核线程函数*/
typedef void thread_func(void*);
/*定义pid_t*/
typedef int16_t pid_t;

/*任务的状态*/
enum task_status
{
  TASK_RUNNING, //0 running
  TASK_READY,   //1 ready
  TASK_BLOCKED, //2 blocked
  TASK_WAITING, //3 waitting
  TASK_HANGING, //4 hang
  TASK_DIED     //5 died
};

/*中断发生时保存上下文的结构*/
struct intr_stack{
  /*低地址,栈顶
   * intr_exit 中断返回时的出栈操作
   * */
  uint32_t vec_no; //kernel.s中断程序中调用具体中断自己入栈的中断向量号
  uint32_t edi;
  uint32_t esi;
  uint32_t ebp;
  uint32_t esp_dummy;
  uint32_t ebx;
  uint32_t edx;
  uint32_t ecx;
  uint32_t eax;
  uint32_t gs;
  uint32_t fs;
  uint32_t es;
  uint32_t ds;

  /*x86中断发生过程cpu自动入栈*/
  uint32_t error_code; 
  void (*eip) (void);
  uint32_t cs;
  uint32_t eflags;
  void *esp;
  uint32_t ss;
  /*高地址,栈底*/
};

/*内核线程切换时保存的上下文结构
 *
 * */
struct thread_stack
{
  uint32_t ebp;
  uint32_t ebx;
  uint32_t edi;
  uint32_t esi;

  void (*eip) (thread_func* func,void *func_arg);

/* 下面的参数只是第一次填充时供调度上cpu,后面的thread栈就不再使用下面的参数了*/
  void (*unused_retaddr); //call调用函数按照C约定栈顶是返回地址,所以这类伪装成返回地址
  thread_func* function;
  void* func_arg;
};

/*PCB,程序控制块*/
struct task_struct
{
  uint32_t *self_kstack;
  pid_t pid;
  enum task_status status;
  char name[16];
  uint8_t priority;

  //2020/4/2:task_struct添加新成员
  uint8_t ticks; //每次上cpu指向的tick数(时间片)
  uint32_t elapsed_ticks;//一共占用了多少cpu的tick数

  /*5-14添加文件描述符数组*/
  int32_t fd_table[MAX_FILES_OPEN_PER_PROC];
  
  struct list_elem general_tag; //双向链表(调度队列)中的节点标签
  struct list_elem all_list_tag;//线程队列(管理所有线程的队列)的节点标签

  //进程自己的页表的虚拟地址
  uint32_t *pgdir;
  
  /*用户进程的虚拟地址空间*/
  struct virtual_addr userprog_vaddr;
  /*2021-5-1: 添加每个用于进程的mem_block_desc[]*/
  struct mem_block_desc u_block_desc[DESC_CNT];
  
  uint32_t cwd_inode_nr;        //进程所在工作目录的inode
  int16_t parent_pid;
  //魔数用来检测pcb栈边界
  uint32_t stack_magic;
};

extern struct list thread_ready_list;
extern struct list thread_all_list;

void thread_create(struct task_struct*,thread_func,void *);
void init_thread(struct task_struct *,char *,int );
struct task_struct *thread_start(char *,int,thread_func,void *);

struct task_struct *running_thread();
void schedule();
void thread_init();

/*21/4/8: 添加锁的时候添加对线程的阻塞和解阻塞*/
void thread_block(enum task_status);
void thread_unblock(struct task_struct *);

void thread_yield();
pid_t fork_pid();
void sys_ps();
#endif
