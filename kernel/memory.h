#ifndef __KERNEL_MEMORY_H
#define __KERNEL_MEMORY_H

#include "stdint.h"
#include "bitmap.h"

/*虚拟内存池表示,2021/3/30:当前只有内核的虚拟内存池*/
enum pool_flags{
  PF_KERNEL = 1,
  PF_USER = 2
};

#define PG_P_1  1 //页项指向的页是否存在于内存中
#define PG_P_0  0
#define PG_RW_R 0 //R/W属性位 可读不可写
#define PG_RW_W 2 //          可读写
#define PG_US_U 4 //US属性位 1表示USER级的页, 0123特权级都能访问
#define PG_US_S 0 //         0表示SUPER级别的页,012特权级能访问

/*
 1. virtual_addr中的bitmap以页为单位,虚拟地址也需要唯一才能和physical地址映射,所以来管理虚拟地址
 ---- 内核的虚拟内存池 2021/3/30----
 * */
struct virtual_addr
{
  struct bitmap vaddr_bitmap; //进程虚拟地址使用的bitmap结构
  uint32_t vaddr_start;       //进程虚拟地址起始地址
};

/*全局变量,内核和用户的物理内存池*/
extern struct pool kernel_pool,user_pool;

/*kernel+user的physical pool+kernel virtual pool初始化 2021/3/30*/
void mem_init();

void *get_kernel_pages(uint32_t);
void *malloc_page(enum pool_flags,uint32_t);
uint32_t *pte_ptr(uint32_t);
uint32_t *pde_ptr(uint32_t);
uint32_t addr_v2p(uint32_t);

/*2021/4/16: 修改memory.c后添加的声明*/
void *get_a_page(enum pool_flags,uint32_t);
void *get_user_pages(uint32_t);

#endif
