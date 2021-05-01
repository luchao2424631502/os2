#include "process.h"
#include "global.h"
#include "debug.h"
#include "memory.h"
#include "thread.h"
#include "list.h"
#include "tss.h"
#include "interrupt.h"
#include "string.h"
#include "console.h"


extern void intr_exit();

/*初始化用户进程上下文*/
void start_process(void *filename_)
{
  void *function = filename_;
  struct task_struct *cur = running_thread();
  cur->self_kstack += sizeof(struct thread_stack);
  struct intr_stack *proc_stack = (struct intr_stack*)cur->self_kstack;

  proc_stack->edi = proc_stack->esi = proc_stack->ebp = proc_stack->esp_dummy = 0;
  proc_stack->ebx = proc_stack->edx = proc_stack->ecx = proc_stack->eax = 0;
  proc_stack->gs = 0;
  proc_stack->ds = proc_stack->es = proc_stack->fs = SELECTOR_U_DATA; 

  proc_stack->eip = function;
  proc_stack->cs = SELECTOR_U_CODE;
  proc_stack->eflags = (EFLAGS_IOPL_0 | EFLAGS_MBS | EFLAGS_IF_1);
  proc_stack->esp = (void*)((uint32_t)get_a_page(PF_USER,USER_STACK3_VADDR) + PG_SIZE);
  proc_stack->ss = SELECTOR_U_DATA;

  /*准备好中断栈环境后,用户进程通过iret进入ring3*/
  asm volatile ("movl %0,%%esp;"
      "jmp intr_exit;"::"g"(proc_stack):"memory");
}

/*cr3切换为thread对应的页目录表,*/
void page_dir_activate(struct task_struct *thread)
{
  //默认赋值为内核线程的页目录表地址
  uint32_t pagedir_phy_addr = 0x100000;
  //说明thread是用户进程
  if (thread->pgdir != NULL)
  {
    pagedir_phy_addr = addr_v2p((uint32_t)thread->pgdir);
  }

  /*更新cr3寄存器*/
  asm volatile ("movl %0,%%cr3"::"r"(pagedir_phy_addr):"memory");
}

/* 1.激活线程/进程的页表
 * 2.更新进程的r0栈指针 esp0
 * */
void process_activate(struct task_struct *thread)
{
  ASSERT(thread != NULL);

  /*激活该进程/线程的页目录表*/
  page_dir_activate(thread);

  /*用户进程*/
  if (thread->pgdir)
  {
    update_tss_esp(thread);
  }
}

/*创建页目录表: 复制当前页目录中内核空间的pde*/
uint32_t *create_page_dir()
{
  uint32_t *page_dir_vaddr = get_kernel_pages(1);

  if (page_dir_vaddr == NULL)
  {
    console_put_str("[error]: userprog/process.c: create_page_dir(): get_kernel_page failed!\n");
    return NULL;
  }

  /*1.先复制页表*/
  memcpy((uint32_t *)((uint32_t)page_dir_vaddr + 0x300*4),(uint32_t *)(0xfffff000 + 0x300*4),1024);

  /*2.更新最后一项 指向自己的物理地址*/
  uint32_t new_page_dir_phy_addr = addr_v2p((uint32_t)page_dir_vaddr);
  page_dir_vaddr[1023] = new_page_dir_phy_addr | PG_US_U | PG_RW_W | PG_P_1; //构造成页目录项
  
  return page_dir_vaddr;
}

/*创建用户进程的虚拟地址的bitmap,(每一个用户进程都有一个)
 * kernel的虚拟地址bitmap定义在memory.c中(全局唯一)
 * */
void create_user_vaddr_bitmap(struct task_struct *user_prog)
{
  //gcc i386 进程入口虚拟地址
  user_prog->userprog_vaddr.vaddr_start = USER_VADDR_START;
  uint32_t bitmap_pg_cnt = DIV_ROUND_UP((0xc0000000 - USER_VADDR_START) / PG_SIZE / 8,PG_SIZE);
  user_prog->userprog_vaddr.vaddr_bitmap.bits = get_kernel_pages(bitmap_pg_cnt);
  user_prog->userprog_vaddr.vaddr_bitmap.btmp_bytes_len = (0xc0000000 - USER_VADDR_START) / PG_SIZE / 8;
  bitmap_init(&user_prog->userprog_vaddr.vaddr_bitmap);
}

/*创建用户进程*/
void process_execute(void *filename,char *name)
{
  //申请用户线程的pcb(为什么在内核空间:因为由内核来管理用户进程的信息)
  struct task_struct *thread = get_kernel_pages(1);

  /*准备用户进程信息,上下文等*/
  init_thread(thread,name,DEFAULT_PRIO);
  create_user_vaddr_bitmap(thread);
  thread_create(thread,start_process,filename);
  thread->pgdir = create_page_dir(); //用户进程独有的页目录表(虚拟地址空间)
  /*2021-5-1:初始化mem_block_descs[]*/
  block_desc_init(thread->u_block_desc);

  /*将用户进程加入到调度队列*/
  enum intr_status old_status = intr_disable();

  ASSERT(!elem_find(&thread_ready_list,&thread->general_tag));
  list_append(&thread_ready_list,&thread->general_tag);
  ASSERT(!elem_find(&thread_all_list,&thread->all_list_tag));
  list_append(&thread_all_list,&thread->all_list_tag);

  intr_set_status(old_status);
}

