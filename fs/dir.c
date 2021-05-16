#include "dir.h"
#include "inode.h"
#include "super_block.h"
#include "file.h"
#include "fs.h"
#include "memory.h"
#include "stdio-kernel.h"
#include "string.h"
#include "debug.h"

struct dir root_dir;    //根目录

/*打开根目录*/
void open_root_dir(struct partition *part)
{
  //根据inode得到文件的具体数据
  root_dir.inode = inode_open(part,part->sb->root_inode_no);
  root_dir.dir_pos = 0;//文件项索引
}

/*在分区part打开inode_no号目录项,并且返回struct dir * */
struct dir *dir_open(struct partition *part,uint32_t inode_no)
{
  struct dir *pdir = (struct dir *)sys_malloc(sizeof(struct dir));
  pdir->inode = inode_open(part,inode_no);
  pdir->dir_pos = 0;
  return pdir;
}

/*在分区part的pdir目录下搜索名字为name的文件/目录*/
bool search_dir_entry(struct partition *part,struct dir *pdir,
    const char *name,struct dir_entry *dir_e)
{
  uint32_t block_cnt = 140;   //12个直接块+128个块(来自inode一级索引表)

  uint32_t *all_blocks = (uint32_t *)sys_malloc(560);//alloc内存来放每个块的地址
  if (all_blocks == NULL)
  {
    printk("fs/dir.c search_dir_entry(): sys_malloc() for all_blocks failed");
    return false;
  }

  uint32_t block_idx = 0;
  while (block_idx < 12)
  {
    all_blocks[block_idx] = pdir->inode->i_sectors[block_idx];
    block_idx++;
  }
  block_idx = 0;

  if (pdir->inode->i_sectors[12] != 0)//将一级间接表的内容都读入内存中
  {
    ide_read(part->my_disk,pdir->inode->i_sectors[12],all_blocks + 12,1);
  }

  uint8_t *buf = (uint8_t *)sys_malloc(SECTOR_SIZE);
  struct dir_entry *p_de = (struct dir_entry *)buf; 
  uint32_t dir_entry_size = part->sb->dir_entry_size;
  uint32_t dir_entry_cnt = SECTOR_SIZE / dir_entry_size;

  //在目录项文件的所有数据(块)中查看name文件/目录
  while (block_idx < block_cnt)
  {
    if (all_blocks[block_idx] == 0)
    {
      block_idx++;
      continue;
    }
    ide_read(part->my_disk,all_blocks[block_idx],buf,1);

    uint32_t dir_entry_idx = 0;
    while (dir_entry_idx < dir_entry_cnt)
    {
      if (!strcmp(p_de->filename,name))
      {
        memcpy(dir_e,p_de,dir_entry_size);
        sys_free(buf);
        sys_free(all_blocks);
        return true;
      }
      dir_entry_idx++;    //index++
      p_de++;     //指向当前扇区(buf)中的下一个目录项
    }
    block_idx++;
    p_de = (struct dir_entry *)buf;
    memset(buf,0,SECTOR_SIZE);
  }

  //执行到这里说明没有找到name项
  sys_free(buf);
  sys_free(all_blocks);
  return false;
}

/*关闭(删除)目录*/
void dir_close(struct dir *dir)
{
  
  if (dir == &root_dir)
  {
    return ;
  }
  inode_close(dir->inode);
  sys_free(dir);
}

/*填充(创建)目录项*/
void create_dir_entry(char *filename,uint32_t inode_no,uint8_t file_type,struct dir_entry *p_de)
{
  ASSERT(strlen(filename) <= MAX_FILE_NAME_LEN);

  memcpy(p_de->filename,filename,strlen(filename));
  p_de->i_no = inode_no;
  p_de->f_type = file_type;
}

/*将目录项p_de写入父目录文件parent_dir中*/
bool sync_dir_entry(struct dir *parent_dir,struct dir_entry *p_de,void *io_buf)
{
  struct inode *dir_inode = parent_dir->inode;
  uint32_t dir_size = dir_inode->i_size;
  uint32_t dir_entry_size = cur_part->sb->dir_entry_size;   //默认的目录项大小

  ASSERT(dir_size % dir_entry_size == 0);

  uint32_t dir_entrys_per_sec = (512 / dir_entry_size);   //一个扇区能放多少个目录项
  int32_t block_lba = -1;

  uint8_t block_idx = 0;
  uint32_t all_blocks[140] = {0};   //140个inode扇区地址

  //暂时只复制了12个直接索引
  while (block_idx < 12)
  {
    all_blocks[block_idx] = dir_inode->i_sectors[block_idx];
    block_idx++;
  }

  struct dir_entry *dir_e = (struct dir_entry *)io_buf;
  int32_t block_bitmap_idx = -1;

  block_idx = 0;
  while (block_idx < 140)
  {
    block_bitmap_idx = -1;
    //在找到第一个非空block前 处理中间空的block
    if (all_blocks[block_idx] == 0)
    {
      block_lba = block_bitmap_alloc(cur_part);//分配一个block
      if (block_lba == -1)
      {
        printk("fs/dir.c sync_to_entry():alloc block bitmap failed\n");
        return false;
      }

      //将内存中已经修改过的block bitmap写入到硬盘中相应位置
      block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
      ASSERT(block_bitmap_idx != -1);
      bitmap_sync(cur_part,block_bitmap_idx,BLOCK_BITMAP);

      block_bitmap_idx = -1;
      if (block_idx < 12)//肯定是直接块啊???
      {
        //应为当前是=0的空闲entry,所以申请了一个block,并且填入dir entry
        dir_inode->i_sectors[block_idx] = all_blocks[block_idx] = block_lba;
      }
      else if (block_idx == 12)//父dir的entry满了,要在一级索引里申请block
      {
        dir_inode->i_sectors[12] = block_lba;//此块作为一级块索引表
        block_lba = -1;
        block_lba = block_bitmap_alloc(cur_part);//在分配一个块作为数据块
        if (block_lba == -1)//再申请一个数据块失败,则要项一级块索引表删除
        {
          block_bitmap_idx = dir_inode->i_sectors[12] - cur_part->sb->data_start_lba;
          bitmap_set(&cur_part->block_bitmap,block_bitmap_idx,0);
          dir_inode->i_sectors[12] = 0;

          printk("fs/dir.c sync_to_entry():alloc block bitmap failed\n");
          return false;
        }

        //将内存中的bitmap同步到硬盘中
        block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
        ASSERT(block_bitmap_idx != -1);
        bitmap_sync(cur_part,block_bitmap_idx,BLOCK_BITMAP);

        all_blocks[12] = block_lba;//数据块

        ide_write(cur_part->my_disk,dir_inode->i_sectors[12],all_blocks+12,1);//将一级block的新块写入一级block表扇区
      }
      else//已经有了一级block索引表,但是表中的block没有分配
      {
        all_blocks[block_idx] = block_lba;
        ide_write(cur_part->my_disk,dir_inode->i_sectors[12],all_blocks+12,1);//同上
      }
      memset(io_buf,0,512);
      /*将p_de(子目录项)写入到 父目录的新block中*/
      memcpy(io_buf,p_de,dir_entry_size);
      ide_write(cur_part->my_disk,all_blocks[block_idx],io_buf,1);
      dir_inode->i_size += dir_entry_size;
      return true;
    }
    
    //这里才是真正查找 已经存在的block中的空闲目录项,然后将p_de(子目录项)写入
    ide_read(cur_part->my_disk,all_blocks[block_idx],io_buf,1);//将block读入到内存中
    uint8_t dir_entry_idx = 0;

    while (dir_entry_idx < dir_entrys_per_sec)
    {
      //ft_unknown的目录填充(因为认为此项是空的
      if ((dir_e + dir_entry_idx)->f_type == FT_UNKNOWN)
      {
        //将目录项写入父目录文件的项中,注意dir_e是指向io_buf的,此时项还在内存中,
        memcpy(dir_e + dir_entry_idx,p_de,dir_entry_size);
        ide_write(cur_part->my_disk,all_blocks[block_idx],io_buf,1);

        dir_inode->i_size += dir_entry_size;
        return true;
      }
      dir_entry_idx++;
    }
    block_idx++;
  }
  printk("directory is full!\n");
  return false;
}
