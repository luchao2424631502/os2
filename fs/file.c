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
#include "list.h"
#include "bitmap.h"

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

/*创建文件,失败返回-1,成功返回fd*/
int32_t file_create(struct dir *parent_dir,char *filename,uint8_t flag)
{
  void *io_buf = sys_malloc(1024);
  if (io_buf == NULL)
  {
    printk("in file_create(): sys_malloc for io_buf failed\n");
    return -1;
  }

  uint8_t rollback_step = 0;

  //为新文件分配inode
  int32_t inode_no = inode_bitmap_alloc(cur_part);
  if (inode_no == -1)
  {
    printk("in file_create(): allocate inode failed\n");
    return -1;
  }

  struct inode *new_file_inode = (struct inode *)sys_malloc(sizeof(struct inode));
  if (new_file_inode == NULL)
  {
    printk("file_crate: sys_malloc for inode failed\n");
    rollback_step = 1;
    goto rollback;
  }

  //初始化inode
  inode_init(inode_no,new_file_inode);

  //在file_table[]中找空闲slot,返回下标
  int fd_idx = get_free_slot_in_global();
  if (fd_idx == -1)
  {
    printk("exceed max open files\n");
    rollback_step = 2;
    goto rollback;
  }

  file_table[fd_idx].fd_inode = new_file_inode;
  file_table[fd_idx].fd_pos = 0;
  file_table[fd_idx].fd_flag = flag;//O_CREATE
  file_table[fd_idx].fd_inode->write_deny = false;

  //目录项
  struct dir_entry new_dir_entry;
  memset(&new_dir_entry,0,sizeof(struct dir_entry));

  //创建目录项
  create_dir_entry(filename,inode_no,FT_REGULAR,&new_dir_entry);


  /*同步数据到硬盘中*/
  if (!sync_dir_entry(parent_dir,&new_dir_entry,io_buf))
  {
    printk("sync dir_entry to disk failed\n");
    rollback_step = 3;
    goto rollback;
  }

  memset(io_buf,0,1024);
  inode_sync(cur_part,parent_dir->inode,io_buf);//将父目录的inode内容填充到硬盘中

  memset(io_buf,0,1024);
  inode_sync(cur_part,new_file_inode,io_buf);//将新文件的Inode内容同步到硬盘中

  bitmap_sync(cur_part,inode_no,INODE_BITMAP);//将inode_bitmap同步到硬盘中

  //将新的Inode添加到内存缓冲中
  list_push(&cur_part->open_inodes,&new_file_inode->inode_tag);
  new_file_inode->i_open_cnts = 1;

  sys_free(io_buf);
  /*将file的全局描述符下标 安装到 进程的pcb的file_table[]中*/
  return pcb_fd_install(fd_idx);

rollback:
  switch(rollback_step)
  {
    case 3://清空全局file_table[]的相应位
      memset(&file_table[fd_idx],0,sizeof(struct file));
    case 2:
      sys_free(new_file_inode);
    case 1:
      bitmap_set(&cur_part->inode_bitmap,inode_no,0);
      break;
  }
  sys_free(io_buf);
  return -1;
}

/*打开inode为inode_no对应的文件,失败返回-1*/
int32_t file_open(uint32_t inode_no,uint8_t flag)
{
  //在全局fd_table[]中找到空闲槽并且返回下标
  int fd_idx = get_free_slot_in_global();
  if (fd_idx == -1)
  {
    printk("fs/file.c file_open(): exceed max open files\n");
    return -1;
  }

  //填充fd_table
  file_table[fd_idx].fd_inode = inode_open(cur_part,inode_no);
  file_table[fd_idx].fd_pos = 0;
  file_table[fd_idx].fd_flag = flag; 

  bool *write_deny = &file_table[fd_idx].fd_inode->write_deny; 

  //写文件需要考虑 write_deny,可以多个进程读,但是不能多个进程同时写
  if (flag & O_WRONLY || flag & O_RDWR)
  {
    //关中断避免调度
    enum intr_status old_status = intr_disable();
    if (!(*write_deny))//当前没有进程在写此文件
    {
      *write_deny = true; 
      intr_set_status(old_status);
    }
    else//其它进程正在写,那么当前进程不能写 
    {
      intr_set_status(old_status);
      printk("fs/file.c file_open(): file can't be write now,try later\n");
      return -1;
    }
  }

  //安装到当前进程的pcb中
  return pcb_fd_install(fd_idx);
}

/*关闭文件*/
int32_t file_close(struct file *file)
{
  if (file == NULL)
    return -1;

  file->fd_inode->write_deny = false;//false写标志
  inode_close(file->fd_inode);      //关闭inode
  file->fd_inode = NULL;            //释放file struct 
  return 0;
}
