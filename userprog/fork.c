#include "fork.h"
#include "thread.h"
#include "memory.h"
#include "global.h"
#include "string.h"
#include "debug.h"
#include "bitmap.h"
#include "file.h"
#include "interrupt.h"
#include "list.h"
#include "process.h"

extern void intr_exit();

/*拷贝父进程的pcb,以及虚拟地址池位图*/
static int32_t copy_pcb_vaddrbitmap_stack0(struct task_struct *child_thread,struct task_struct *parent_thread)
{
  //1.复制父进程的pcb到子进程中
  memcpy(child_thread,parent_thread,PG_SIZE);//拷贝pcb
  child_thread->pid = fork_pid();
  child_thread->elapsed_ticks = 0;
  child_thread->status = TASK_READY;
  child_thread->ticks = child_thread->priority;
  child_thread->parent_pid = parent_thread->pid;
  child_thread->general_tag.prev = child_thread->general_tag.next = NULL;
  child_thread->all_list_tag.prev = child_thread->all_list_tag.next = NULL;
  block_desc_init(child_thread->u_block_desc);

  //2.复制父进程的虚拟地址池的位图
  uint32_t bitmap_pg_cnt = DIV_ROUND_UP((0xc0000000 - USER_VADDR_START) / PG_SIZE / 8,PG_SIZE);
  void *vaddr_btmp = get_kernel_pages(bitmap_pg_cnt);
  if (vaddr_btmp == NULL)
  {
    return -1;
  }

  //将父进程的vaddr_bitmap复制到vaddr_btmp中
  memcpy(vaddr_btmp,child_thread->userprog_vaddr.vaddr_bitmap.bits,bitmap_pg_cnt * PG_SIZE);
  //内容不变,但是是新的内存空间
  child_thread->userprog_vaddr.vaddr_bitmap.bits = vaddr_btmp;

  ASSERT(strlen(child_thread->name) < 11);
  strcat(child_thread->name,"_fork");
  return 0;
}

/*复制父进程的 代码段和数据段 和 栈
 * buf_page:内核空间提供的一段缓冲区
 * */
static void copy_body_stack3(struct task_struct *child_thread,struct task_struct *parent_thread,void *buf_page)
{
  uint8_t *vaddr_btmp = parent_thread->userprog_vaddr.vaddr_bitmap.bits;
  uint32_t btmp_bytes_len = parent_thread->userprog_vaddr.vaddr_bitmap.btmp_bytes_len;
  uint32_t vaddr_start = parent_thread->userprog_vaddr.vaddr_start;
  uint32_t idx_byte = 0;
  uint32_t idx_bit = 0;
  uint32_t prog_vaddr = 0;

  while (idx_byte < btmp_bytes_len)
  {
    if (vaddr_btmp[idx_byte])
    {
      idx_bit = 0;
      while (idx_bit < 8)
      {
        //判断每个字节的每个bit是否存在页
        if ((BITMAP_MASK << idx_bit) & vaddr_btmp[idx_byte])
        {
          prog_vaddr = (idx_byte * 8 + idx_bit) * PG_SIZE + vaddr_start;//此bit对应的具体的页地址

          //将父进程用户空间的数据复制到内核空间内存中,供访问
          memcpy(buf_page,(void *)prog_vaddr,PG_SIZE);

          //cr3切换到子进程的页目录表,目的:将父进程的页(已在内核空间中),复制到子进程的页中
          page_dir_activate(child_thread);

          /*子进程创建一页,接收从父进程复制过来的页数据
          因为bitmap已经是从父进程复制过来的,所以不需要设置bitmap,*/
          get_a_page_without_opvaddrbitmap(PF_USER,prog_vaddr);

          //从内核空间在复制过来(内核空间是作为中转用的)
          memcpy((void *)prog_vaddr,buf_page,PG_SIZE);

          //恢复父进程cr3寄存器(页目录基地址),开始搜寻父进程的下一个页
          page_dir_activate(parent_thread);
        }
        idx_bit++;
      }
    }
    idx_byte++;
  }
}

/*为子进程构建thread_stack
 * 子进程执行是加入到了就绪队列中,则是通过schedule()->switch_to(),
 * 所以要构造thread_stack,
 * 并且返回的地址是intr_exit(从中断返回是因为栈中保存了父进程的上下文,这样子进程也能从fork()处开始执行)
 */
static int32_t build_child_stack(struct task_struct *child_thread)
{
  //拿到intr_stack栈顶,(低地址
  struct intr_stack *intr_0_stack = (struct intr_stack *)((uint32_t)child_thread + PG_SIZE - sizeof(struct intr_stack));
  //1.修改子进程的返回值=0
  intr_0_stack->eax = 0;

  //指向pcb中thread_stack的元素指针
  uint32_t *ret_addr_in_thread_stack = (uint32_t *)intr_0_stack - 1;//eip
  uint32_t *esi_ptr_in_thread_stack = (uint32_t *)intr_0_stack - 2;
  uint32_t *edi_ptr_in_thread_stack = (uint32_t *)intr_0_stack - 3;
  uint32_t *ebx_ptr_in_thread_stack = (uint32_t *)intr_0_stack - 4;
  //thread_stack栈顶
  uint32_t *ebp_ptr_in_thread_stack = (uint32_t *)intr_0_stack - 5;

  //为了子进程从fork处开始执行
  *ret_addr_in_thread_stack = (uint32_t)intr_exit;

  //赋空值就行
  *ebp_ptr_in_thread_stack = *ebx_ptr_in_thread_stack = *edi_ptr_in_thread_stack = *esi_ptr_in_thread_stack = 0;

  //将pcb栈指针赋值为thread_stack的栈顶,供switch_to()做栈恢复时使用
  child_thread->self_kstack = ebp_ptr_in_thread_stack;

  return 0;
}

/*更新inode被进程打开的次数*/
static void update_inode_open_cnts(struct task_struct *thread)
{
  int32_t local_fd = 3,global_fd = 0;
  while (local_fd < MAX_FILES_OPEN_PER_PROC)//每个进程最多打开8个fd
  {
    global_fd = thread->fd_table[local_fd];//拿到file_table的下标
    ASSERT(global_fd < MAX_FILE_OPEN);
    if (global_fd != -1)
    {
      file_table[global_fd].fd_inode->i_open_cnts++;
    }
    local_fd++;
  }
}

/*sys_fork()的主要部分:实现拷贝父进程的所有资源给子进程*/
static int32_t copy_process(struct task_struct *child_thread,struct task_struct *parent_thread)
{
  //在内核地址空间中申请内存作为中转缓冲区
  void *buf_page = get_kernel_pages(1);
  if (buf_page == NULL)
  {
    return -1;
  }

  //a. 拷贝父进程的pcb(包括内核栈),虚拟地址bitmap 到子进程中
  if (copy_pcb_vaddrbitmap_stack0(child_thread,parent_thread) == -1)
  {
    return -1;
  }

  //b. 为子进程创建页表,此页表内容仅包括内核空间
  child_thread->pgdir = create_page_dir();
  if (child_thread->pgdir == NULL)
  {
    return -1;
  }

  //c.复制父进程的所有内存页资源(代码段,数据段,栈段)
  copy_body_stack3(child_thread,parent_thread,buf_page);

  //d. 构建子进程的thread_stack()供switch_to()使用.并且修改返回值
  build_child_stack(child_thread);

  //e.更新文件表中inode的打开次数
  update_inode_open_cnts(child_thread);

  //释放内核空间的内存,
  mfree_page(PF_KERNEL,buf_page,1);
  return 0;
}

/*fork子进程,*/
pid_t sys_fork()
{
  struct task_struct *parent_thread = running_thread();
  //创建子进程的pcb
  struct task_struct *child_thread = get_kernel_pages(1);
  if (child_thread == NULL)
  {
    return -1;
  }
  //当前关中断,并且父进程不是空(有自己的地址空间)
  ASSERT(INTR_OFF == intr_get_status() && parent_thread->pgdir != NULL);

  if (copy_process(child_thread,parent_thread) == -1)
  {
    return -1;
  }

  /*子进程资源继承完毕,加入到就绪队列中即可被调度执行,并且也加入到全部进程队列*/
  ASSERT(!elem_find(&thread_ready_list,&child_thread->general_tag));
  list_append(&thread_ready_list,&child_thread->general_tag);
  ASSERT(!elem_find(&thread_all_list,&child_thread->all_list_tag));
  list_append(&thread_all_list,&child_thread->all_list_tag);

  return child_thread->pid;//父进程返回的是子进程的pid,(子进程的pid已经填好是0,所以也会返回0
}
