#include "memory.h"
#include "bitmap.h"
#include "stdint.h"
#include "global.h"
#include "debug.h"
#include "print.h"
#include "string.h"
#include "sync.h"


/*0xc009f000是内核栈顶,则内核pcb从0xc009e000开始,
 * 一个页框的Bitmap可表示4kb*8*4Kb=128mb大小内存,安排4个页框的bitmap则可表示512mb大小内存
 * 则bitmap的虚拟地址起始地址为0xc009a000
 * */
#define MEM_BITMAP_BASE 0xc009a000

#define PDE_IDX(addr) ((addr & 0xffc00000) >> 22) //求虚拟地址对应的pde的索引
#define PTE_IDX(addr) ((addr & 0x003ff000) >> 12) //求虚拟地址对应的pte的索引

/*内核使用heap的起始虚拟地址*/
#define K_HEAP_START 0xc0100000

struct pool {
  struct bitmap pool_bitmap; //bitmap管理此内存池
  uint32_t phy_addr_start;   //内存池管理物理内存的起始地址
  uint32_t pool_size;        //池大小
  struct lock lock;           //申请内存时互斥
};

//分配4个页给bitmap来管理物理内存,则可管理512mb大小的实际内存,而bochs实际配置的是32mb大小内存

struct pool kernel_pool,user_pool;
struct virtual_addr kernel_vaddr; //管理内核线程的虚拟地址

/*在虚拟内存池中申请pg_cnt个页的内存*/
static void* vaddr_get(enum pool_flags pf,uint32_t pg_cnt)
{
  int vaddr_start = 0,bit_idx_start = -1;
  uint32_t cnt = 0;
  //内核物理内存池
  if (pf == PF_KERNEL)
  {
    //申请pg_cnt个bit
    bit_idx_start = bitmap_scan(&kernel_vaddr.vaddr_bitmap,pg_cnt);
    if (bit_idx_start == -1)
    {
      return NULL;
    }
    while(cnt < pg_cnt)
    {
      //将申请到的bit置为1
      bitmap_set(&kernel_vaddr.vaddr_bitmap,bit_idx_start + cnt++,1);
    }
    vaddr_start = kernel_vaddr.vaddr_start + bit_idx_start * PG_SIZE;
  }
  // 2021/3/30: 当前还没有用户虚拟内存池(只有用户物理内存池),等实现用户进程在实现
  // 2021/4/15: 实现用户进程前的内存池准备
  else
  {
    struct task_struct *cur = running_thread();
    bit_idx_start = bitmap_scan(&cur->userprog_vaddr.vaddr_bitmap,pg_cnt);
    if (bit_idx_start == -1) /*虚拟内存地址空间已经没有内存了*/
    {
      return NULL;
    }

    while (cnt < pg_cnt)
    {
      bitmap_set(&cur->userprog_vaddr.vaddr_bitmap,bit_idx_start + cnt++,1);
    }
    //分配得到的虚拟内存的起始地址
    vaddr_start = cur->userprog_vaddr.vaddr_start + bit_idx_start * PG_SIZE;

    /*用户进程分配的虚拟内存地址不能超过3GB地址的虚拟内存空间*/
    ASSERT((uint32_t)vaddr_start < (0xc0000000 - PG_SIZE));
  }
  return (void*)vaddr_start;
}


/*根据vaddr得到对应的页表项的虚拟地址*/
uint32_t *pte_ptr(uint32_t vaddr)
{
  uint32_t *pte_ptr = (uint32_t *)(0xffc00000 + \
      ((vaddr & 0xffc00000) >> 10) +\
      PTE_IDX(vaddr) * 4);
  return pte_ptr;
}

/*根据vaddr得到对应页目录项的虚拟地址*/
uint32_t *pde_ptr(uint32_t vaddr)
{
  uint32_t *pde = (uint32_t*)((0xfffff000) + PDE_IDX(vaddr) * 4);
  return pde;
}

/* page alloc : 在m_pool(物理内存池)中申请分配1页内存*/
static void *palloc(struct pool *m_pool)
{
  int bit_idx = bitmap_scan(&m_pool->pool_bitmap,1);
  if (bit_idx == -1)
  {
    return NULL;
  }
  bitmap_set(&m_pool->pool_bitmap,bit_idx,1); //bit_idx位 置1
  uint32_t page_phyaddr = ((bit_idx * PG_SIZE) + m_pool->phy_addr_start); //申请到的页的起始地址
  return (void*)page_phyaddr;
}

/*在页表中添加vaddr和phyaddr的映射*/
static void page_table_add(void *_vaddr,void *_page_phyaddr)
{
  uint32_t vaddr = (uint32_t)_vaddr;
  uint32_t page_phyaddr = (uint32_t)_page_phyaddr;
  uint32_t *pde = pde_ptr(vaddr);
  uint32_t *pte = pte_ptr(vaddr);

  //判断虚拟地址所在的页目录项是否存在
  if (*pde & 0x00000001) 
  {
    ASSERT(!(*pte & 0x00000001)); //页表项应该不存在,否则说明分页分配的有问题
    if (!(*pte & 0x00000001))
    {//页表项指向页的物理地址 + 一系列属性
      *pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
    }
    else 
    {
      PANIC("[PANIC] kernel/memory.c page_table_add()");
      // PANIC(!(*pte & 0x1));
      //*pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
    }
  }
  else
  //页目录项不存在,需要先创建页目录项(申请一个页当作页表)
  {
    //申请的页表
    uint32_t pde_phyaddr = (uint32_t)palloc(&kernel_pool);
    //页目录项指向申请到的页表
    *pde = (pde_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
    //新页表要清零,保证页表内容不会导致分页映射混乱
    memset((void*)((int)pte & 0xfffff000),0,PG_SIZE);

    ASSERT(!(*pte & 0x00000001));//当前页表项没有映射关系,来创建
    *pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
  }
}

/* 分配pg_cnt个页,返回虚拟地址:
 * 1.vaddr_get先在虚拟内存池中申请内存空间
 * 2.palloc在物理内存池中申请内存空间
 * 3.page_table_add使得虚拟地址和物理地址建立映射关系
 * */
void *malloc_page(enum pool_flags pf,uint32_t pg_cnt)
{
  //大小限制为15mb,内核物理内存池也就15mb大小
  ASSERT(pg_cnt > 0 && pg_cnt < 3840);
  void* vaddr_start = vaddr_get(pf,pg_cnt);
  if (vaddr_start == NULL)
  {
    return NULL;
  }
  
  uint32_t vaddr = (uint32_t)vaddr_start;
  uint32_t cnt = pg_cnt;
  struct pool *mem_pool = pf & PF_KERNEL ? &kernel_pool : &user_pool;
  //虚拟地址是连续的可物理地址不是连续的
  while(cnt-- > 0)
  {
    void *page_phyaddr = palloc(mem_pool);
    if (page_phyaddr == NULL)
    {
      return NULL;
    }
    page_table_add((void*)vaddr,page_phyaddr);
    vaddr += PG_SIZE;
  }
  return vaddr_start;
}

/*从内存物理池分配pg_cnt个页,返回虚拟地址*/
void *get_kernel_pages(uint32_t pg_cnt)
{
  lock_acquire(&kernel_pool.lock);
  void *vaddr = malloc_page(PF_KERNEL,pg_cnt);
  if (vaddr != NULL)
  {
    memset(vaddr,0,pg_cnt * PG_SIZE);
  }
  lock_release(&kernel_pool.lock);
  return vaddr;
}

/*在用户空间中申请4K内存,返回虚拟地址*/
void *get_user_pages(uint32_t pg_cnt)
{
  lock_acquire(&user_pool.lock);
  void *vaddr = malloc_page(PF_USER,pg_cnt);
  memset(vaddr,0,pg_cnt * PG_SIZE);
  lock_release(&user_pool.lock);
  return vaddr;
}


/*申请1页内存,并且由参数来指定虚拟地址*/
void *get_a_page(enum pool_flags pf,uint32_t vaddr)
{
  struct pool *mem_pool = pf & PF_KERNEL ? &kernel_pool : &user_pool;
  lock_acquire(&mem_pool->lock);

  /*拿到当前进程/线程的pcb地址,先将管理的虚拟内存对应bit置1*/
  struct task_struct *cur = running_thread();
  int32_t bit_idx = -1;

  /*用户进程*/
  if (cur->pgdir != NULL && pf == PF_USER)
  {
    //要指定的虚拟内存地址 - 用户进程起始的虚拟内存地址
    bit_idx = (vaddr - cur->userprog_vaddr.vaddr_start) / PG_SIZE;
    ASSERT(bit_idx > 0);
    bitmap_set(&cur->userprog_vaddr.vaddr_bitmap,bit_idx,1);
  }
  /*内核线程*/
  else if (cur->pgdir == NULL && pf == PF_KERNEL)
  {
    bit_idx = (vaddr - kernel_vaddr.vaddr_start) / PG_SIZE;
    ASSERT(bit_idx > 0);
    bitmap_set(&kernel_vaddr.vaddr_bitmap,bit_idx,1);
  }
  else 
  {
    PANIC("[PANIC]: kernel/memory.c: get_a_page(): not allow kernel alloc userspace or user alloc kernelspace by get_a_page()");
  }

  //申请物理内存
  void *page_phyaddr = palloc(mem_pool); 
  if (page_phyaddr == NULL)
  {
    return NULL;
  }
  //添加映射(因为是我们手动指定的虚拟地址)
  page_table_add((void*)vaddr,page_phyaddr);
  lock_release(&mem_pool->lock);

  return (void *)vaddr;
}

/*虚拟地址转换为物理地址*/
uint32_t addr_v2p(uint32_t vaddr)
{
  //得到页表项(内容是 页的物理地址)的虚拟地址,
  uint32_t *pte = pte_ptr(vaddr);
  return ((*pte & 0xfffff000) + (vaddr & 0x00000fff));
}


/* ---- 内存池初始化 ----
 *  1. 实际内存的管理:
 *      1) 内核程序需要申请内存,所以物理内存需要分给一部分给内核,按照1:1分实际内存,则需要一个pool来管理内核实际内存分配
 *      2) 如上,就要有个内核的虚拟内存分配管理
 *      3) user层进程使用的实际内存空间管理
 *      4) 目前没有user进程,所以没有user进程的虚拟内存管理,并且user进程有多个,而内核的虚拟内存管理只需要一个pool来.
 *  2. 如上,目前就有3个pool需要初始化,2个管理实际物理内存,1个管理内核的虚拟内存分配,
 *      kernel的物理内存管理和kernel的虚拟内存管理是对应的,
 * */
static void mem_pool_init(uint32_t all_mem)
{
  put_str("    [mem_pool_init] start\n");
  /* 页目录+
   * (后面的)第一个页表(低256项对应1mb内存)+
   * (后面的)第769-1022个页目录项对应的254个页
   * */
  uint32_t page_table_size = PG_SIZE * 256;
  /*已经使用的物理内存大小=低端1MB+页表(页目录)占用总和页
   * */
  uint32_t used_mem = page_table_size + 0x100000;
  uint32_t free_mem = all_mem - used_mem;
  /*剩下可以使用的物理内存能凑成多少页*/
  uint16_t all_free_pages = free_mem / PG_SIZE;
  /*实际内存由内核和用户1:1平分*/
  uint16_t kernel_free_pages = all_free_pages / 2;
  uint16_t user_free_pages = all_free_pages - kernel_free_pages;
  
  /*bitmap需要多少个字节来表示*/
  uint32_t kbm_length = kernel_free_pages / 8;
  uint32_t ubm_length = user_free_pages / 8;

  /*used_mem既是已经使用的内存,也是剩下可用内存的起始地址*/
  uint32_t kp_start = used_mem; 
  uint32_t up_start = kp_start + kernel_free_pages * PG_SIZE;

  /*给内存池赋初值*/
  kernel_pool.phy_addr_start = kp_start;
  user_pool.phy_addr_start = up_start;

  kernel_pool.pool_size = kernel_free_pages * PG_SIZE;
  user_pool.pool_size = user_free_pages * PG_SIZE;
  
  kernel_pool.pool_bitmap.btmp_bytes_len = kbm_length;
  user_pool.pool_bitmap.btmp_bytes_len = ubm_length;
  
  /* kernel和user池中bitmap放置在实际内存中的位置
   * 0xc009a000提供了4个页来放bitmap,先放kernel的bitmap
   * */
  kernel_pool.pool_bitmap.bits = (void*)MEM_BITMAP_BASE;
  /*接着放置user物理内存pool的Bitmap*/
  user_pool.pool_bitmap.bits = (void*)(MEM_BITMAP_BASE + kbm_length);
  
  put_str("        kernel_pool_bitmap_start: "); put_int((int)kernel_pool.pool_bitmap.bits); put_char('\n');
  put_str("        kernel_pool_phy_addr_start: "); put_int(kernel_pool.phy_addr_start); put_char('\n');
  put_str("        user_pool_bitmap_start: "); put_int((int)user_pool.pool_bitmap.bits); put_char('\n');
  put_str("        user_pool_phy_addr_start:  "); put_int(user_pool.phy_addr_start); put_char('\n');

  /*管理物理内存前,清理bitmap */
  bitmap_init(&kernel_pool.pool_bitmap);
  bitmap_init(&user_pool.pool_bitmap);

  /* 2021/4/15: 实现用户进程修改memory.c时pool添加了lock成员.所以要初始化*/
  lock_init(&kernel_pool.lock);
  lock_init(&user_pool.lock);

  /*
   * 实际内存管理池初始化完成,现在来初始化内核进程的虚拟内存池管理
   * 因为elf内核image解析到0xc0015000处执行,所以这个虚拟内存池管理内核程序的申请内存问题
   * */
  kernel_vaddr.vaddr_bitmap.btmp_bytes_len = kbm_length;
  /*内核heap的bitmap放在kernel pool bitmap和user pool bitmap后面*/
  kernel_vaddr.vaddr_bitmap.bits = (void*)(MEM_BITMAP_BASE + kbm_length + ubm_length);
  /*内核heap虚拟地址的起始分配点*/
  kernel_vaddr.vaddr_start = K_HEAP_START;
  
  /*
   *管理kernel虚拟内存池先,清理bitmap */
  bitmap_init(&kernel_vaddr.vaddr_bitmap);

  put_str("    [mem_pool_init] done\n");
}

void mem_init()
{
  put_str("[mem_init] start\n");
  /*注意loader中已经将读取的内存容量存放到物理地址0xb00处*/
  mem_pool_init(*(uint32_t*)0xb00);
  put_str("[mem_init] done\n");
}
