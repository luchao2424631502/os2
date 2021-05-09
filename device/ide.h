#ifndef __DEVICE_IDE_H
#define __DEVICE_IDE_H

#include "stdint.h"
#include "sync.h"
#include "list.h"
#include "bitmap.h"

/* 分区结构
 * */
struct partition
{
  uint32_t start_lba;         //起始扇区
  uint32_t sec_cnt;           //扇区数
  struct disk *my_disk;       //分区所属硬盘
  struct list_elem part_tag;  
  char name[8];               //分区名称
  struct super_block *sb;     //分区超级块
  struct bitmap block_bitmap; //块bitmap
  struct bitmap inode_bitmap; //inode bitmap
  struct list open_inodes;    //i节点队列
};

/*硬盘*/
struct disk 
{
  char name[8];                   //硬盘名称
  struct ide_channel *my_channel; //硬盘所属通道
  uint8_t dev_no;                 //硬盘分配的设备号
  struct partition prim_parts[4]; //
  struct partition logic_parts[8];//逻辑分区(应该是无限个),这里支持8个逻辑分区
};

/*IDE通道*/
struct ide_channel
{
  char name[8];
  uint16_t port_base;         //通道起始端口
  uint8_t irq_no;             //通道所用中断号
  struct lock lock;           //通道锁
  bool expecting_intr;        //等待硬盘中断
  struct semaphore disk_done; //硬盘工作时,将驱动程序阻塞
  struct disk devices[2];     //一个通道可以接2个硬盘
};

void intr_hd_handler(uint8_t);
void ide_init();
extern uint8_t channel_cnt;
extern struct ide_channel channels[];
void ide_read(struct disk *,uint32_t,void *,uint32_t);
void ide_write(struct disk *,uint32_t ,void *,uint32_t );
#endif
