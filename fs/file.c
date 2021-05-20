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
#include "ide.h"

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

/**/
int32_t file_write(struct file *file,const void *buf,uint32_t count)
{
  //Inode块支持的最大文件是140*512字节
  if ((file->fd_inode->i_size + count) > (BLOCK_SIZE * 140))
  {
    printk("fs/file.c file_write(): exceed max file_size 512*140 bytes,write file failed\n");
    return -1;
  }

  uint8_t *io_buf = sys_malloc(BLOCK_SIZE);
  if (io_buf == NULL)
  {
    printk("fs/file.c file_write(): sys_malloc for io_buf failed\n");
    return -1;
  }

  //记录文件的所有块地址
  uint32_t *all_blocks = (uint32_t *)sys_malloc(BLOCK_SIZE + 48);
  if (all_blocks == NULL)
  {
    printk("fs/file.c file_write(): sys_malloc for all_blocks failed\n");
    return -1;
  }

  const uint8_t *src = buf;
  uint32_t bytes_written = 0;       //已经写入的数据
  uint32_t size_left = count;       //剩下未写入的数据
  int32_t block_lba = -1;           //块地址
  uint32_t block_bitmap_idx = 0;    //block在block_bitmap中的位置
  uint32_t sec_idx;                 //扇区索引
  uint32_t sec_lba;                 //
  uint32_t sec_off_bytes;           //扇区内字节偏移量
  uint32_t sec_left_bytes;          //扇区内剩下的字节 
  uint32_t chunk_size;              //
  int32_t indirect_block_table;     //一级Inode表地址
  uint32_t block_idx;               //块索引


  /*文件是第一次写*/
  if (file->fd_inode->i_sectors[0] == 0)
  {
    block_lba = block_bitmap_alloc(cur_part);
    if (block_lba == -1)
    {
      printk("fs/file.c file_write(): block_bitmap_alloc() failed");
      return -1;
    }
    file->fd_inode->i_sectors[0] = block_lba;//分配了一个新块

    /*将内存中的bitmap写入的硬盘中(因为bitmap被修改了*/
    block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
    ASSERT(block_bitmap_idx != 0);
    bitmap_sync(cur_part,block_bitmap_idx,BLOCK_BITMAP);
  }

  //文件已经占用的扇区数量
  uint32_t file_has_used_blocks = file->fd_inode->i_size / BLOCK_SIZE + 1;

  //文件将要占用的扇区数量
  uint32_t file_will_use_blocks = (file->fd_inode->i_size + count) / BLOCK_SIZE + 1;
  ASSERT(file_will_use_blocks <= 140);//小于最大支持的文件大小

  uint32_t add_blocks = file_will_use_blocks - file_has_used_blocks;

  if (add_blocks == 0)//增量扇区数=0
  {
    if (file_has_used_blocks <= 12)//数据量在12 sec以内
    {
      block_idx = file_has_used_blocks - 1;
      all_blocks[block_idx] = file->fd_inode->i_sectors[block_idx];
    }
    else//数据在间接表中 
    {
      //确保间接表的扇区地址不是空
      ASSERT(file->fd_inode->i_sectors[12] != 0);
      indirect_block_table = file->fd_inode->i_sectors[12];
      //将一级间接表中的512字节读入到all_blocks中存储
      ide_read(cur_part->my_disk,indirect_block_table,all_blocks + 12,1);
    }
  }
  /*写入文件的数据的增量用到了新的扇区*/
  else 
  {
    //1.即使增量数据使用新扇区,但总量还是在12个扇区内
    if (file_will_use_blocks <= 12)
    {
      block_idx = file_has_used_blocks - 1;
      ASSERT(file->fd_inode->i_sectors[block_idx] != 0);//已经占用的扇区的地址肯定不是空
      all_blocks[block_idx] = file->fd_inode->i_sectors[block_idx];

      block_idx = file_has_used_blocks;//第一个使用的新扇区的索引
      while (block_idx < file_will_use_blocks)//处理所有的新增扇区
      {
        //申请block(扇区)
        block_lba = block_bitmap_alloc(cur_part);
        if (block_lba == -1)
        {
          printk("fs/file.c file_write(): block_bitmap_alloc() for situation 1 failed\n");
          return -1;
        }

        ASSERT(file->fd_inode->i_sectors[block_idx] == 0);//肯定是未分配的地址
        //既写到all_blocks[]内存中,也写到inode struct中的i_sectors[]中
        file->fd_inode->i_sectors[block_idx] = all_blocks[block_idx] = block_lba;

        //将内存中的block bitmap(当前分配的了新的block)写入到内存中
        block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
        bitmap_sync(cur_part,block_bitmap_idx,BLOCK_BITMAP);

        block_idx++;
      }
    }
    //2.原本文件大小在12扇区内,但是增量占用到了1级间接表的扇区内
    else if (file_has_used_blocks <= 12 && file_will_use_blocks > 12)
    {
      block_idx = file_has_used_blocks - 1;//旧数据使用的最后一个扇区的索引
      all_blocks[block_idx] = file->fd_inode->i_sectors[block_idx];//写入到内存中暂存者

      //创建一级间接表
      block_lba = block_bitmap_alloc(cur_part);
      if (block_lba == -1)
      {
        printk("fs/file.c file_write(): block_bitmap_alloc() for situation 2 failed\n");
        return -1;
      }

      ASSERT(file->fd_inode->i_sectors[12] == 0);//之前肯定是没有被分配的
      indirect_block_table = file->fd_inode->i_sectors[12] = block_lba;

      block_idx = file_has_used_blocks;//第一个未使用的扇区的idx,
      while (block_idx < file_will_use_blocks)
      {
        //申请的新的扇区
        block_lba = block_bitmap_alloc(cur_part);
        if (block_lba == -1)
        {
          printk("fs/file.c file_write(): block_bitmap_alloc() for situation 2 failed\n");
          return -1;
        }

        if (block_idx < 12)//在一级间接表外面
        {
          //一定是尚未分配的地址
          ASSERT(file->fd_inode->i_sectors[block_idx] == 0);
          file->fd_inode->i_sectors[block_idx] = all_blocks[block_idx] = block_lba;
        }
        //在一级间接表中
        else 
        {
          //属于间接表的地址暂时都在all_blocks[]中,等待一次性写入file->fd_inode->i_sectors[12]
          all_blocks[block_idx] = block_lba;
        }

        //因为分配的新的block,所以内存中的bitmap会改变,将改变的bitmap写入硬盘中对应的位置
        block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
        bitmap_sync(cur_part,block_bitmap_idx,BLOCK_BITMAP);

        block_idx++;
      }
      //现在是一次性写入all_blocks中存储的一级间接表的地址
      ide_write(cur_part->my_disk,indirect_block_table,all_blocks + 12,1);
    }
    //3.原本文件的大小已经超过12个扇区(也就是已经在1级间接表中,则增量直接在一级间接表中操作
    else if (file_has_used_blocks > 12)
    {
      ASSERT(file->fd_inode->i_sectors[12] != 0);//既然超过12个扇区了,那么一级间接表的lba肯定不是空
      indirect_block_table = file->fd_inode->i_sectors[12];//得到一级间接表的lba

      //从硬盘中读取到内存all_blocks[]中
      ide_read(cur_part->my_disk,indirect_block_table,all_blocks+12,1);

      block_idx = file_has_used_blocks;//第一个未使用的扇区的idx
      while (block_idx < file_will_use_blocks)
      {
        block_lba = block_bitmap_alloc(cur_part);
        if (block_lba == -1)
        {
          printk("fs/file.c file_write(): block_bitmap_alloc() for situation 3 failed\n");
          return -1;
        }
        all_blocks[block_idx++] = block_lba;

        //将内存中的bitmap写入到硬盘中
        block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
        bitmap_sync(cur_part,block_bitmap_idx,BLOCK_BITMAP);
      }
      ide_write(cur_part->my_disk,indirect_block_table,all_blocks + 12,1);//将一级间接表地址写入到硬盘中
    }
  }

  //开始正式写,因为文件数据的lba都拿到内存中来了,
  bool first_write_block = true;//含有剩余空间的标识
  file->fd_pos = file->fd_inode->i_size - 1;//这里是追加写入的情况
  while (bytes_written < count)
  {
    memset(io_buf,0,BLOCK_SIZE);
    sec_idx = file->fd_inode->i_size / BLOCK_SIZE; 
    sec_lba = all_blocks[sec_idx];
    sec_off_bytes = file->fd_inode->i_size % BLOCK_SIZE;
    sec_left_bytes = BLOCK_SIZE - sec_off_bytes; 

    //这次写入的数据量
    chunk_size = size_left < sec_left_bytes ? size_left : sec_left_bytes;
    /*第一次写入时,可能写入的不是完整扇区,
     * 所以先要将扇区原来的数据读入,
     * 直接写入扇区会导致原来的数据丢失*/
    if (first_write_block)
    {
      ide_read(cur_part->my_disk,sec_lba,io_buf,1);
      first_write_block = false;
    }
    //src = buf,是待写入的数据
    memcpy(io_buf + sec_off_bytes,src,chunk_size);
    ide_write(cur_part->my_disk,sec_lba,io_buf,1);
    //待注释
    printk("file write at lba 0x%x\n",sec_lba);

    src += chunk_size;
    file->fd_inode->i_size += chunk_size;
    file->fd_pos += chunk_size;
    bytes_written += chunk_size;
    size_left -= chunk_size;
  }
  //将内存中的此文件的Inode同步到硬盘中对应的inode位置
  inode_sync(cur_part,file->fd_inode,io_buf);
  sys_free(all_blocks);
  sys_free(io_buf);
  return bytes_written;
}

/*从文件file中读取count个字节,并且写入buf中,返回读取到的字节数,失败返回-1*/
int32_t file_read(struct file *file,void *buf,uint32_t count)
{
  uint8_t *buf_dst = (uint8_t *)buf;
  uint32_t size = count;      //要写入的字节数量
  uint32_t size_left = size;  //剩余要写入的字节数量

  //要读取的字节超过了剩下的字节
  if ((file->fd_pos + count) > file->fd_inode->i_size)
  {
    size = file->fd_inode->i_size - file->fd_pos;
    size_left = size;
    if (size == 0)
      return -1;
  }

  uint8_t *io_buf = sys_malloc(BLOCK_SIZE);
  if (io_buf == NULL)
  {
    printk("fs/file.c file_read(): sys_malloc for io_buf failed\n");
  }

  //文件所有块的Lba
  uint32_t *all_blocks = (uint32_t *)sys_malloc(BLOCK_SIZE + 48);
  if (all_blocks == NULL)
  {
    printk("fs/file.c file_read(): sys_malloc for all_blocks failed\n");
    return -1;
  }

  uint32_t block_read_start_idx = file->fd_pos / BLOCK_SIZE;//要读取的数据块所在的扇区起始idx
  uint32_t block_read_end_idx = (file->fd_pos + size) / BLOCK_SIZE;
  // uint32_t read_blocks = block_read_start_idx - block_read_end_idx;
  uint32_t read_blocks = block_read_end_idx - block_read_start_idx;
  ASSERT(block_read_start_idx < 139 && block_read_end_idx < 139);

  int32_t indirect_block_table;   //存储间接表的地址
  uint32_t block_idx;             //待读取的块的idx

  //只需要读取一个扇区(file->fd_pos所在扇区
  if (read_blocks == 0)
  {
    ASSERT(block_read_end_idx == block_read_start_idx);
    //判断是否在12个直接块内,还是在一级间接表中
    if (block_read_end_idx < 12)
    {
      block_idx = block_read_end_idx;
      all_blocks[block_idx] = file->fd_inode->i_sectors[block_idx];
    }
    else
    {
      indirect_block_table = file->fd_inode->i_sectors[12];
      //从硬盘的indirect_block_table处读取一个扇区内容到all_blocks+12处
      ide_read(cur_part->my_disk,indirect_block_table,all_blocks + 12,1);
    }
  }
  //要读取文件的多个块
  else 
  {
    //都在12个直接块内
    if (block_read_end_idx < 12)
    {
      block_idx = block_read_start_idx;
      while (block_idx <= block_read_end_idx)
      {
        all_blocks[block_idx] = file->fd_inode->i_sectors[block_idx];
        block_idx++;
      }
    }
    //一部分在12个直接块内,另一个部分在一级间接表中
    else if (block_read_start_idx < 12 && block_read_end_idx >= 12)
    {
      block_idx = block_read_start_idx;
      //先读取直接块
      while (block_idx < 12)
      {
        all_blocks[block_idx] = file->fd_inode->i_sectors[block_idx];
        block_idx++;
      }
      //既然另一部分在间接表,则间接表地址肯定不是空
      ASSERT(file->fd_inode->i_sectors[12] != 0);

      indirect_block_table = file->fd_inode->i_sectors[12];
      ide_read(cur_part->my_disk,indirect_block_table,all_blocks + 12,1);
    }
    //要读取的数据都在间接表中,
    else 
    {
      ASSERT(file->fd_inode->i_sectors[12] != 0);
      indirect_block_table = file->fd_inode->i_sectors[12];
      ide_read(cur_part->my_disk,indirect_block_table,all_blocks+12,1);
    }
  }

  /*正式开始读取*/
  uint32_t sec_idx,sec_lba,sec_off_bytes,sec_left_bytes,chunk_size;
  uint32_t bytes_read = 0;    //已经读取到的数据字节数
  while (bytes_read < size)
  {
    sec_idx = file->fd_pos / BLOCK_SIZE;
    sec_lba = all_blocks[sec_idx];
    sec_off_bytes = file->fd_pos % BLOCK_SIZE;//fd_pos在最后一个扇区中占用了多少字节(偏移量
    sec_left_bytes = BLOCK_SIZE - sec_off_bytes;//此扇区剩余空闲字节数 
    chunk_size = size_left < sec_left_bytes ? size_left : sec_left_bytes;

    memset(io_buf,0,BLOCK_SIZE);
    ide_read(cur_part->my_disk,sec_lba,io_buf,1);
    //除了第一次外,后面sec_off_bytes = 0,
    memcpy(buf_dst,io_buf + sec_off_bytes,chunk_size);

    buf_dst += chunk_size;
    file->fd_pos += chunk_size;
    bytes_read += chunk_size;
    size_left -= chunk_size;
  }

  sys_free(all_blocks);
  sys_free(io_buf);
  return bytes_read;
}
