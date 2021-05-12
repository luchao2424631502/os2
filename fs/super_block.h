#ifndef __FS_SUPER_BLOCK_H
#define __FS_SUPER_BLOCK_H

#include "stdint.h"

/*分区上的文件系统的元信息*/
struct super_block
{
  uint32_t magic;         //分区的文件系统魔数
  uint32_t sec_cnt;       //分区的总扇区数量
  uint32_t inode_cnt;     //分区inode节点的总数量
  uint32_t part_lba_base; //分区起始的lba地址
  
  uint32_t block_bitmap_lba;  //空闲块bitmap在磁盘的Lba
  uint32_t block_bitmap_sects;//空闲块bitmap占用的扇区数量

  uint32_t inode_bitmap_lba;  //inode_bitmap在磁盘的起始lba
  uint32_t inode_bitmap_sects;//inode_bitmap占用磁盘的扇区数量

  uint32_t inode_table_lba;   //inode table在磁盘的起始lba
  uint32_t inode_table_sects; //inode table占用磁盘扇区数量

  uint32_t data_start_lba;    //数据区开始的Lba地址
  uint32_t root_inode_no;     //root根目录的inode
  uint32_t dir_entry_size;    //目录项大小
  
  uint8_t nouse[460];
} __attribute__((packed));

#endif
