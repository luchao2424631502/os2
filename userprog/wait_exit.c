#include "wait_exit.h"
#include "global.h"
#include "debug.h"
#include "thread.h"
#include "list.h"
#include "memory.h"
#include "fs.h"
#include "file.h"
#include "pipe.h"

/* 释放用户进程资源:
 * 1. 进程自己页表中对应的物理页(进程占用的物理内存)
 * 2. 进程的虚拟内存池 占用的页
 * 3. 打开的文件
 * */
static void release_prog_resource(struct task_struct *release_thread)
{
  uint32_t *pgdir_vaddr = release_thread->pgdir;

  uint16_t user_pde_nr = 768,pde_idx = 0;
  uint32_t pde = 0;
  uint32_t *v_pde_ptr = NULL;

  uint16_t user_pte_nr = 1024,pte_idx = 0;
  uint32_t pte = 0;
  uint32_t *v_pte_ptr = NULL;

  uint32_t *first_pte_vaddr_in_pde = NULL;
  uint32_t pg_phy_addr = 0;

  /*1.回收用户空间的页*/
  while (pde_idx < user_pde_nr)//遍历用户空间占用的3/4的页目录项
  {
    v_pde_ptr = pgdir_vaddr + pde_idx;//得到当前页目录项的地址
    pde = *v_pde_ptr;                 //得到页目录项内容

    if (pde & 0x1)//页目录项存在,则对应的页表内有存在的页表项
    {
      //根据是第几个页表得到地址,然后得到第一个页表项地址
      first_pte_vaddr_in_pde = pte_ptr(pde_idx * 0x400000);
      pte_idx = 0;
      //遍历页表的所有页表项
      while (pte_idx < user_pte_nr)
      {
        v_pte_ptr = first_pte_vaddr_in_pde + pte_idx;
        pte = *v_pte_ptr;
        if (pte & 0x1)
        {
          //拿到页的物理地址
          pg_phy_addr = pte & 0xfffff000;
          //根据物理地址所属于的空间,清除对应池的bitmap,等于释放内存
          free_a_phy_page(pg_phy_addr);
        }
        pte_idx++;
      }
      //释放页表(4k)自己
      pg_phy_addr = pde & 0xfffff000;
      free_a_phy_page(pg_phy_addr);
    }
    pde_idx++;
  }

  /*2.回收虚拟内存池(pool)占用的物理内存*/
  uint32_t bitmap_pg_cnt = (release_thread->userprog_vaddr.vaddr_bitmap.btmp_bytes_len) / PG_SIZE;
  uint8_t *user_vaddr_pool_bitmap = release_thread->userprog_vaddr.vaddr_bitmap.bits;
  mfree_page(PF_KERNEL,user_vaddr_pool_bitmap,bitmap_pg_cnt);

  uint8_t local_fd = 3;
  //MAX_FILES_OPEN_PER_PROC = 8(每个进程最多打开8个文件描述符)
  while (local_fd < MAX_FILES_OPEN_PER_PROC)
  {
    if (release_thread->fd_table[local_fd] != -1)
    {
      if (is_pipe(local_fd))
      {
        uint32_t global_fd = fd_local2global(local_fd);
        if (--file_table[global_fd].fd_pos == 0)
        {
          mfree_page(PF_KERNEL,file_table[global_fd].fd_inode,1);
          file_table[global_fd].fd_inode = NULL;
        }
      }
      else 
      {
        sys_close(local_fd);
      }
    }
    local_fd++;
  }
}

/*遍历list的回调函数*/
static bool find_child(struct list_elem *elem,int32_t ppid)
{
  struct task_struct *thread = elem2entry(struct task_struct,all_list_tag,elem);
  if (thread->parent_pid == ppid)
    return true;
  return false;
}

/*遍历list的回调函数*/
static bool find_hanging_child(struct list_elem *elem,int32_t ppid)
{
  struct task_struct *thread = elem2entry(struct task_struct,all_list_tag,elem);
  if (thread->parent_pid == ppid && thread->status == TASK_HANGING)
    return true;
  return false;
}

/*init进程收养子进程(收养孤儿进程)*/
static bool init_adopt_a_child(struct list_elem *elem,int32_t pid)
{
  struct task_struct *thread = elem2entry(struct task_struct,all_list_tag,elem);
  //此进程的父进程是pid,则退出
  if (thread->parent_pid == pid)
  {
    //修改此进程的父进程为init进程
    thread->parent_pid = 1;
  }
  return false;
}

/*
 * 等待子进程调用exit,从内核中获取子进程结束状态,
 * 成功返回子进程pid,失败返回-1
 * */
pid_t sys_wait(int32_t *status)
{
  struct task_struct *parent_thread = running_thread();

  while (1)
  {
    //1.首先找 子进程是否处于 挂起态
    struct list_elem *child_elem = list_traversal(&thread_all_list,find_hanging_child,parent_thread->pid);
    if (child_elem != NULL)
    {
      //拿到子进程的task_struct
      struct task_struct *child_thread = elem2entry(struct task_struct,all_list_tag,child_elem);
      *status = child_thread->exit_status;

      uint16_t child_pid = child_thread->pid;

      //释放进程的pcb和页目录表,从调度队列中除掉
      thread_exit(child_thread,false);

      return child_pid;//子进程退出成功(资源都释放掉了)
    }

    //判断子进程是否存在
    child_elem = list_traversal(&thread_all_list,find_child,parent_thread->pid);
    if (child_elem == NULL)
    {
      return -1;
    }
    else
    {
      thread_block(TASK_WAITING);
    }
  }
}

/*子进程结束自己的调用*/
void sys_exit(int32_t status)
{
  struct task_struct *child_thread = running_thread();
  child_thread->exit_status = status;
  if (child_thread->parent_pid == -1)
  {
    PANIC("userprog/wait_exit.c sys_exit(): child_thread->parent_pid is -1\n");
  }

  /*那么自己的子进程都变成了孤儿进程,将自己的子进程的父进程改为init*/
  list_traversal(&thread_all_list,init_adopt_a_child,child_thread->pid);

  /*释放自己的资源*/
  release_prog_resource(child_thread);

  /*判断父进程是否在wait我(此时父进程处于waiting状态,想从内核拿到我的资源)*/
  struct task_struct *parent_thread = pid2thread(child_thread->parent_pid);
  if (parent_thread->status == TASK_WAITING)
  {
    //父进程开始新一轮查找hanging的子进程
    thread_unblock(parent_thread);
  }

  //如果父进程没有结束并且没有wait我,那么hanging的子进程就是僵尸进程(pcb未释放,但是资源已经释放)
  thread_block(TASK_HANGING);
}
