#ifndef __FS_INODE_H
#define __FS_INODE_H

#include "stdint.h"
#include "list.h"

struct inode
{
  uint32_t i_no;        //inode
  uint32_t i_size;      //文件大小 or 目录项大小和
  uint32_t i_open_cnts; //inode被打开的次数
  bool write_deny;      //写 互斥

  /*15个=12+1(1)+2(2),这里只用到一个一级inode索引表*/
  uint32_t i_sectors[13];
  struct list_elem inode_tag;//已经在内存的inode队列
};

#endif
