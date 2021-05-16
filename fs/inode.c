#include "inode.h"
#include "debug.h"
#include "super_block.h"
#include "string.h"
#include "ide.h"
#include "list.h"
#include "thread.h"
#include "memory.h"
#include "interrupt.h"

//记录inode在磁盘的位置
struct inode_position 
{
  bool two_sec;
  uint32_t sec_lba;
  uint32_t off_size;
};

/*根据分区和inode_no找到inode所在的扇区Lba和偏移*/
static void inode_locate(struct partition *part,uint32_t inode_no,
    struct inode_position *inode_pos)
{
  ASSERT(inode_no < 4096);
  uint32_t inode_table_lba = part->sb->inode_table_lba;

  uint32_t inode_size = sizeof(struct inode);
  uint32_t off_size = inode_no * inode_size;
  uint32_t off_sec = off_size / 512;
  uint32_t off_size_in_sec = off_size % 512;

  uint32_t left_in_sec = 512 - off_size_in_sec;
  //struct inode跨扇区了
  if (left_in_sec < inode_size)
  {
    inode_pos->two_sec = true;
  }
  else 
  {
    inode_pos->two_sec = false;
  }

  inode_pos->sec_lba = inode_table_lba + off_sec;
  inode_pos->off_size = off_size_in_sec;
}

/*将inode 写入part*/
void inode_sync(struct partition *part,struct inode *inode,void *io_buf)
{
  uint8_t inode_no = inode->i_no;
  struct inode_position inode_pos;
  inode_locate(part,inode_no,&inode_pos);
  ASSERT(inode_pos.sec_lba <= (part->start_lba + part->sec_cnt));

  struct inode pure_inode; 
  memcpy(&pure_inode,inode,sizeof(struct inode));

  //3项在内存中才使用到的项
  pure_inode.i_open_cnts = 0;
  pure_inode.write_deny = false;//inode下次读到内存使用时 表示可写
  pure_inode.inode_tag.prev = pure_inode.inode_tag.next = NULL;

  char *inode_buf = (char *)io_buf;
  if (inode_pos.two_sec)//inode结构跨扇区
  {
    ide_read(part->my_disk,inode_pos.sec_lba,inode_buf,2);
    memcpy((inode_buf+inode_pos.off_size),&pure_inode,sizeof(struct inode));
    ide_write(part->my_disk,inode_pos.sec_lba,inode_buf,2);
  }
  //仅在一个扇区中
  else
  {
    ide_read(part->my_disk,inode_pos.sec_lba,inode_buf,1);
    memcpy((inode_buf+inode_pos.off_size),&pure_inode,sizeof(struct inode));
    ide_write(part->my_disk,inode_pos.sec_lba,inode_buf,1);
  }
}

/*给出inode_no返回inode struct*/
struct inode *inode_open(struct partition *part,uint32_t inode_no)
{
  //先在内存的inode链表缓冲区中找,找不到在从磁盘中拿出来
  struct list_elem *elem = part->open_inodes.head.next;
  struct inode *inode_found;
  while (elem != &part->open_inodes.tail)
  {
    inode_found = elem2entry(struct inode,inode_tag,elem);
    if (inode_found->i_no == inode_no)
    {
      inode_found->i_open_cnts++;
      return inode_found;
    }
    elem = elem->next;
  }

  //进行到这里说明找不到
  struct inode_position inode_pos;
  inode_locate(part,inode_no,&inode_pos);
  
  struct task_struct *cur = running_thread();
  uint32_t *cur_pagedir_bak = cur->pgdir;
  cur->pgdir = NULL;
  //将进程自己的页表指针置为0,则在内核内存空间中分配
  inode_found = (struct inode*)sys_malloc(sizeof(struct inode));
  cur->pgdir = cur_pagedir_bak;

  char *inode_buf;
  if (inode_pos.two_sec)
  {
    inode_buf = (char *)sys_malloc(1024);

    ide_read(part->my_disk,inode_pos.sec_lba,inode_buf,2);
  }
  else
  {
    inode_buf = (char *)sys_malloc(512);

    ide_read(part->my_disk,inode_pos.sec_lba,inode_buf,1);
  }
  //从磁盘中找到了inode_no对应的inode struct
  memcpy(inode_found,inode_buf + inode_pos.off_size,sizeof(struct inode));

  list_push(&part->open_inodes,&inode_found->inode_tag);
  inode_found->i_open_cnts = 1;//在内存中是++,而这是第一次打开(因为是从磁盘中读出来的)

  sys_free(inode_buf);
  return inode_found;
}

/*关闭inode*/
void inode_close(struct inode *inode)
{
  enum intr_status old_status = intr_disable();
  if (--inode->i_open_cnts == 0)
  {
    //先将inode从part维护的open_inodes链表中去掉
    list_remove(&inode->inode_tag);

    struct task_struct *cur = running_thread();
    uint32_t *cur_pagedir_bak = cur->pgdir;
    cur->pgdir = NULL;
    sys_free(inode);
    cur->pgdir = cur_pagedir_bak;
  }
  intr_set_status(old_status);
}

/*初始化inode*/
void inode_init(uint32_t inode_no,struct inode *new_inode)
{
  new_inode->i_no = inode_no;
  new_inode->i_size = 0;
  new_inode->i_open_cnts = 0;
  new_inode->write_deny = false;

  uint8_t sec_idx = 0;
  while (sec_idx < 13)
  {
    new_inode->i_sectors[sec_idx] = 0;
    sec_idx++;
  }
}
