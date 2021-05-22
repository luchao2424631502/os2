#include "thread.h"
#include "stdint.h"
#include "string.h"
#include "global.h"
#include "debug.h"
#include "interrupt.h"
#include "print.h"
#include "memory.h"
#include "process.h"
#include "sync.h"
#include "main.h"

extern void switch_to(struct task_struct *,struct task_struct *);

struct task_struct *main_thread; //主线程的PCB
struct task_struct *idle_thread; //idle线程
struct list thread_ready_list;   //线程就绪队列
struct list thread_all_list;     //所有线程队列
static struct list_elem *thread_tag;//保存队列中的线程节点
struct lock pid_lock;            //pid锁

/*空闲线程*/
static void idle(void *arg UNUSED)
{
  while (1)
  {
    thread_block(TASK_BLOCKED);
    //开中断使得cpu可以从暂停状态恢复
    asm volatile ("sti; hlt":::"memory");
  }
}


/*获取当前线程pcb指针*/
struct task_struct *running_thread()
{
  uint32_t esp;
  asm ("mov %%esp,%0":"=g"(esp)::);
  //根据当前线程的栈esp得到pcb的起始地址
  return (struct task_struct *)(esp & 0xfffff000);
}

static void kernel_thread(thread_func* function,void *func_arg)
{
  //避免时钟中断被屏蔽,后面此线程一直独占cpu
  intr_enable();
  function(func_arg);
}

/*给task分配pid*/
static pid_t allocate_pid()
{
  /*局部作用域下的静态变量*/
  static pid_t next_pid = 0;
  /*pid_lock在thread_init中初始化*/
  lock_acquire(&pid_lock);
  next_pid++;
  lock_release(&pid_lock);
  return next_pid;
}

/*分配pid*/
pid_t fork_pid()
{
  return allocate_pid();
}

/*初始化线程栈,*/
void thread_create(struct task_struct *thread,thread_func function,void *func_arg)
{
  //esp 目前在高地址PCB的最后一个字节处,预留一个中断栈大小空间
  thread->self_kstack -= sizeof(struct intr_stack);
  thread->self_kstack -= sizeof(struct thread_stack);
  
  struct thread_stack *kthread_stack = (struct thread_stack*)thread->self_kstack;
  //填充线程栈的内容,为ret进入function做准备
  kthread_stack->eip = kernel_thread;
  kthread_stack->function = function;
  kthread_stack->func_arg = func_arg;
  kthread_stack->ebp = kthread_stack->ebx \
                       = kthread_stack->edi = kthread_stack->esi = 0;
}


/*初始化线程基本信息*/
void init_thread(struct task_struct *thread,char *name,int prio)
{
  memset(thread,0,sizeof(*thread));
  //2021-4-29分配task的pid
  thread->pid = allocate_pid();
  strcpy(thread->name,name);
  //把main函数封装成线程
  if (thread == main_thread)
  {
    thread->status = TASK_RUNNING;
  }
  else
  {
    thread->status = TASK_READY;
  }

  thread->priority = prio;
  //栈顶在PCB的最后一个字节
  thread->self_kstack = (uint32_t*)((uint32_t)thread + PG_SIZE);
  thread->ticks = prio;
  thread->elapsed_ticks = 0;
  thread->pgdir = NULL;

  //预留stdio的描述符
  thread->fd_table[0] = 0;
  thread->fd_table[1] = 1;
  thread->fd_table[2] = 2;
  uint8_t fd_idx = 3;
  while(fd_idx < MAX_FILES_OPEN_PER_PROC)
  {
    thread->fd_table[fd_idx] = -1;
    fd_idx++;
  }

  thread->cwd_inode_nr = 0;//根目录作为默认的工作路径
  thread->parent_pid = -1;
  thread->stack_magic = 0x20010522;
}

struct task_struct *thread_start(char *name,int prio,thread_func function,void *func_arg)
{
  //给线程的PCB分配内存(说白了就是给线程栈空间和OS管理它的信息)
  struct task_struct *thread = get_kernel_pages(1); 
  //初始化线程信息
  init_thread(thread,name,prio);
  //初始化线程栈
  thread_create(thread,function,func_arg);

  //由于要运行,加入到准备队列中
  ASSERT(!elem_find(&thread_ready_list,&thread->general_tag));
  list_append(&thread_ready_list,&thread->general_tag);

  //加入到全体线程链表中
  ASSERT(!elem_find(&thread_all_list,&thread->all_list_tag));
  list_append(&thread_all_list,&thread->all_list_tag);

  /*
  asm volatile ("movl %0,%%esp;"
                "pop %%ebp;"
                "pop %%ebx;"
                "pop %%edi;"
                "pop %%esi;"
                "ret;"
      :
      : "g"(thread->self_kstack)
      : "memory");
  */
  return thread;
}

/*将kernel中的main函数完善成主线程*/
static void make_main_thread()
{
  main_thread = running_thread();
  init_thread(main_thread,"main",31);
  //检测main_thread是否在thread_all_list中(肯定是不在的)
  ASSERT(!elem_find(&thread_all_list,&main_thread->all_list_tag));
  //将main加入到thread_all_list中
  list_append(&thread_all_list,&main_thread->all_list_tag);
}


/*线程调度,0x20 中断->intr_timer_handler->schedule(ticks==0)
 * 2021/4/16: 调度器增加对用户进程的支持(添加cr3和栈切换)
 * */
void schedule()
{
  /*现在位于中断处理程序中,必须是关中断*/
  ASSERT(intr_get_status() == INTR_OFF);
  
  /*由esp栈指针拿到当前pcb的虚拟地址*/
  struct task_struct *cur_task = running_thread();
  if (cur_task->status == TASK_RUNNING)
  {
    //肯定不在就绪队列中
    ASSERT(!elem_find(&thread_ready_list,&cur_task->general_tag));
    list_append(&thread_ready_list,&cur_task->general_tag);
    //时间片用尽了,恢复时间片,供下次调度使用
    cur_task->ticks = cur_task->priority;
    //修改task的状态,由running->ready,等待下次调度上cpu执行
    cur_task->status = TASK_READY;
  }
  //当前task的状态不是runing(可能陷入某等待条件)
  else
  {

  }

  /*当前就绪队列为空,唤醒idle线程*/
  if (list_empty(&thread_ready_list))
  {
    thread_unblock(idle_thread);
  }

  ASSERT(!list_empty(&thread_ready_list));
  thread_tag = NULL;
  thread_tag = list_pop(&thread_ready_list);
  //next_task 的 pcb地址
  struct task_struct *next_task = elem2entry(struct task_struct,general_tag,thread_tag);
  next_task->status = TASK_RUNNING;

  /**/
  process_activate(next_task);

  switch_to(cur_task,next_task);
}

/*线程将自己阻塞,并且将状态改为status*/
void thread_block(enum task_status status)
{
  ASSERT((status == TASK_BLOCKED || status == TASK_WAITING || status == TASK_HANGING));
  enum intr_status old_status = intr_disable();
  struct task_struct *cur_thread = running_thread();
  cur_thread->status = status;
  schedule(); //当前线程被阻塞,调用调度器来换ready队列上的线程来执行
  intr_set_status(old_status);
}

/*将某个线程取消阻塞*/
void thread_unblock(struct task_struct *thread)
{
  enum intr_status old_status = intr_disable();
  ASSERT((thread->status == TASK_BLOCKED)||(thread->status == TASK_WAITING)||(thread->status == TASK_HANGING));
  //只要thread的状态不是ready,说明当前不再就绪队列,仍然是被阻塞
  if (thread->status != TASK_READY)
  {
    ASSERT(!elem_find(&thread_ready_list,&thread->general_tag));
    //如果在线程就绪队列说明调度有问题
    if (elem_find(&thread_ready_list,&thread->general_tag))
    {
      PANIC("[PANIC]: thread.c/thread_unblock(): BLOCKED thread is in thread_ready_list\n");
    }
    //现在来加入线程就绪队列,并且是队列前部(取消线程的阻塞)
    list_push(&thread_ready_list,&thread->general_tag);
    thread->status = TASK_READY;
  }
  intr_set_status(old_status);
}

/*当前线程让出cpu时间片,立刻加入到就绪队列,(注意不是阻塞)*/
void thread_yield()
{
  struct task_struct *cur = running_thread();
  enum intr_status old_status = intr_disable();
  ASSERT(!elem_find(&thread_ready_list,&cur->general_tag));
  list_append(&thread_ready_list,&cur->general_tag);
  cur->status = TASK_READY;
  schedule();
  intr_set_status(old_status);
}

/*内核线程初始化(调度器)运行前准备*/
void thread_init()
{
  put_str("[thread_init] start\n");

  list_init(&thread_ready_list);
  list_init(&thread_all_list);
  //2021-4-29 初始化allocate_pid()中的pid_lock
  lock_init(&pid_lock);

  /*2021-5-22: 在main和idle线程创建前先创建init用户进程,是第一个用户进程,也是所有用户进程的父亲*/
  process_execute(init,"init");

  //将main函数包装成线程
  make_main_thread();

  //添加空闲线程
  idle_thread = thread_start("IDLE",10,idle,NULL);

  put_str("[thread_init] end\n");
}
