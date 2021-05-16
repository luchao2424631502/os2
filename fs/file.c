#include "file.h"
#include "fs.h"
#include "super_block.h"
#include "inode.h"
#include "stdio-kernel.h"
#include "memory.h"
#include "debug.h"
#include "interrupt.h"
#include "string.h"
#include "thread.h"
#include "global.h"

#define DEFAULT_SECS 1

/* 文件表 */
struct file file_table[MAX_FILE_OPEN];

/* 从文件表file_table中获取一个空闲位,成功返回下标,失败返回-1 */
int32_t get_free_slot_in_global(void) {
   uint32_t fd_idx = 3;
   while (fd_idx < MAX_FILE_OPEN) {
      if (file_table[fd_idx].fd_inode == NULL) {
	 break;
      }
      fd_idx++;
   }
   if (fd_idx == MAX_FILE_OPEN) {
      printk("exceed max open files\n");
      return -1;
   }
   return fd_idx;
}

/* 将全局描述符下标安装到进程或线程自己的文件描述符数组fd_table中,
 * 成功返回下标,失败返回-1 */
int32_t pcb_fd_install(int32_t globa_fd_idx) {
   struct task_struct* cur = running_thread();
   uint8_t local_fd_idx = 3; // 跨过stdin,stdout,stderr
   while (local_fd_idx < MAX_FILES_OPEN_PER_PROC) {
      if (cur->fd_table[local_fd_idx] == -1) {	// -1表示free_slot,可用
	 cur->fd_table[local_fd_idx] = globa_fd_idx;
	 break;
      }
      local_fd_idx++;
   }
   if (local_fd_idx == MAX_FILES_OPEN_PER_PROC) {
      printk("exceed max open files_per_proc\n");
      return -1;
   }
   return local_fd_idx;
}

/* 分配一个i结点,返回i结点号 */
int32_t inode_bitmap_alloc(struct partition* part) {
   int32_t bit_idx = bitmap_scan(&part->inode_bitmap, 1);
   if (bit_idx == -1) {
      return -1;
   }
   bitmap_set(&part->inode_bitmap, bit_idx, 1);
   return bit_idx;
}

/*分配一个扇区,返回lba*/
int32_t block_bitmap_alloc(struct partition *part)
{
  int32_t bit_idx = bitmap_scan(&part->block_bitmap,1);
  if (bit_idx == -1)
  {
    return -1;
  }

  bitmap_set(&part->block_bitmap,bit_idx,1);
  return (part->sb->data_start_lba + bit_idx);
}

/* 将bitmap中的bit_idx位的512Byte写入对应的硬盘位置 
 * */
void bitmap_sync(struct partition *part,uint32_t bit_idx,uint8_t btmp_type)
{
  uint32_t off_sec  = bit_idx / 4096;       //bit_idx偏移的扇区个数
  uint32_t off_size = off_sec * BLOCK_SIZE; //这些扇区所占的BYTE大小
  uint32_t sec_lba;     //磁盘中对应的bit的lba
  uint8_t *bitmap_off;  //内存中的bitmap起始地址

  switch(btmp_type)
  {
    case INODE_BITMAP:
      sec_lba = part->sb->inode_bitmap_lba + off_sec;
      bitmap_off = part->inode_bitmap.bits + off_size;
      break;
    case BLOCK_BITMAP:
      sec_lba = part->sb->block_bitmap_lba + off_sec;
      bitmap_off = part->block_bitmap.bits + off_size;
      break;
  }

  //将bitmap_off处的512Byte写入对应的bitmap扇区
  ide_write(part->my_disk,sec_lba,bitmap_off,1);
}
