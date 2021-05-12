#include "fs.h"
#include "inode.h"
#include "super_block.h"
#include "dir.h"
#include "ide.h"
#include "global.h"
#include "stdio-kernel.h"
#include "memory.h"
#include "debug.h"
#include "string.h"

/*分区格式化*/
// static void partition_format(struct disk *hd,struct partition *part)
static void partition_format(struct partition *part)
{
  uint32_t boot_sector_sects = 1;
  uint32_t super_block_sects = 1;
  uint32_t inode_bitmap_sects = DIV_ROUND_UP(MAX_FILES_PER_PART,BITS_PER_SECTOR); 
  uint32_t inode_table_sects = DIV_ROUND_UP(((sizeof(struct inode) * MAX_FILES_PER_PART)),SECTOR_SIZE);
  uint32_t used_sects = boot_sector_sects + super_block_sects + inode_bitmap_sects + inode_table_sects;
  uint32_t free_sects = part->sec_cnt - used_sects;

  //空闲块要在其它信息块占用后来计算大小
  uint32_t block_bitmap_sects;
  block_bitmap_sects = DIV_ROUND_UP(free_sects,BITS_PER_SECTOR);
  //空闲块的个数=free-空闲块bitmap占用的扇区数
  uint32_t block_bitmap_bit_len = free_sects - block_bitmap_sects;
  block_bitmap_sects = DIV_ROUND_UP(block_bitmap_bit_len,BITS_PER_SECTOR);

  /*超级块初始化*/
  struct super_block sb;
  sb.magic = 0x20010522;
  sb.sec_cnt = part->sec_cnt;
  sb.inode_cnt = MAX_FILES_PER_PART;
  sb.part_lba_base = part->start_lba;

  sb.block_bitmap_lba = sb.part_lba_base + 2;
  sb.block_bitmap_sects = block_bitmap_sects;

  sb.inode_bitmap_lba = sb.block_bitmap_lba + sb.block_bitmap_sects;
  sb.inode_bitmap_sects = inode_bitmap_sects;

  sb.inode_table_lba = sb.inode_bitmap_lba + sb.inode_bitmap_sects;
  sb.inode_table_sects = inode_table_sects;

  sb.data_start_lba = sb.inode_table_lba + sb.inode_table_sects;
  sb.root_inode_no = 0;
  sb.dir_entry_size = sizeof(struct dir_entry);

  printk("%s info:\n",part->name);
  printk("   magic:0x%x\n   part_lba_base:0x%x\n   all_sectors:0x%x\n   inode_cnt:0x%x\n   block_bitmap_lba:0x%x\n   block_bitmap_sectors:0x%x\n   inode_bitmap_lba:0x%x\n   inode_bitmap_sectors:0x%x\n   inode_table_lba:0x%x\n   inode_table_sectors:0x%x\n   data_start_lba:0x%x\n", sb.magic, sb.part_lba_base, sb.sec_cnt, sb.inode_cnt, sb.block_bitmap_lba, sb.block_bitmap_sects, sb.inode_bitmap_lba, sb.inode_bitmap_sects, sb.inode_table_lba, sb.inode_table_sects, sb.data_start_lba);

  struct disk *hd = part->my_disk;
  /*1,将superblock写入分区起始的第一个扇区*/
  ide_write(hd,part->start_lba+1,&sb,1);
  printk("  super_block_lba:0x%x\n",part->start_lba + 1);

  /*2.将bitmap初始化然后写入sb.block_bitmap_lba(对应磁盘的扇区),*/
  uint32_t buf_size = (sb.block_bitmap_sects >= sb.inode_bitmap_sects ? sb.block_bitmap_sects:sb.inode_bitmap_sects);
  buf_size = (buf_size >= sb.inode_table_sects ? buf_size:sb.inode_table_sects)*SECTOR_SIZE;//buf_size肯定是Inode_table的大小 
  uint8_t *buf = (uint8_t *)sys_malloc(buf_size);

  //初始化block bitmap,所以第一个扇区是给根目录使用,
  buf[0] |= 0x01; 
  uint32_t block_bitmap_last_byte = block_bitmap_bit_len / 8;
  uint8_t block_bitmap_last_bit = block_bitmap_bit_len % 8;
  uint32_t last_size = SECTOR_SIZE - (block_bitmap_last_byte % SECTOR_SIZE);//block bitmap的最后一个扇区的未使用的字节

  memset(&buf[block_bitmap_last_byte],0xff,last_size);

  uint8_t bit_idx = 0;
  while (bit_idx <= block_bitmap_last_bit)
  {
    //将磁盘最后的可用的block对应的bitmap的Bit置为0
    buf[block_bitmap_last_byte] &= ~(1 << bit_idx++);
  }
  // ide_write(hd,part->start_lba + 2,buf,sb.block_bitmap_sects);
  ide_write(hd,sb.block_bitmap_lba,buf,sb.block_bitmap_sects);


  /*3.将inode bitmap初始化,然后写入sb.inode_bitmap_lba(磁盘对应处)*/
  memset(buf,0,buf_size);
  buf[0] |= 0x1;
  ide_write(hd,sb.inode_bitmap_lba,buf,sb.inode_bitmap_sects);

  /*4.初始化inode table并且写入sb.inode_table_lba*/
  memset(buf,0,buf_size);
  struct inode *i=(struct inode *)buf; 
  
  //先准备根目录的inode,因为已经在空的block的第一项准备了struct inode
  i->i_size = 2 * sb.dir_entry_size;// . ..
  i->i_no = 0;
  i->i_sectors[0] = sb.data_start_lba;//第一个空闲block的lba
  ide_write(hd,sb.inode_table_lba,buf,sb.inode_table_sects);

  /*5.初始化第一个空闲block,根目录实际数据(2个dir entry)*/
  memset(buf,0,buf_size);
  struct dir_entry *p_de = (struct dir_entry *)buf; 

  //先初始化 .
  memcpy(p_de->filename,".",1);
  p_de->i_no = 0;
  p_de->f_type = FT_DIRECTORY; 
  p_de++;

  //然后初始化 ..
  memcpy(p_de->filename,"..",2);
  p_de->i_no = 0;
  p_de->f_type = FT_DIRECTORY;

  ide_write(hd,sb.data_start_lba,buf,1);

  printk("  root_dir_lba:0x%x\n",sb.data_start_lba);
  printk("%s format done\n",part->name);
  sys_free(buf);
}

/*文件系统初始化*/
void filesys_init()
{
  uint8_t channel_no = 0;
  uint8_t dev_no;
  uint8_t part_idx = 0;

  struct super_block *sb_buf = (struct super_block*)sys_malloc(SECTOR_SIZE);
  if (sb_buf == NULL)
  {
    PANIC("fs.c/filesys_init(): alloc memory failed!");
  }
  printk("searching filesystem\n");
  while (channel_no < channel_cnt)
  {
    dev_no = 0;
    while (dev_no < 2)
    {
      //跨国hd60M.img
      if (dev_no == 0)
      {
        dev_no++;
        continue;
      }
      struct disk *hd = &channels[channel_no].devices[dev_no];
      struct partition *part = hd->prim_parts;
      while (part_idx < 12)
      {
        if (part_idx == 4)
        {
          part = hd->logic_parts;
        }
        if (part->sec_cnt != 0)
        {
          memset(sb_buf,0,SECTOR_SIZE);
          ide_read(hd,part->start_lba+1,sb_buf,1);

          if (sb_buf->magic == 0x20010522)
            printk("%s has filesystem\n",part->name);
          else
          {
            printk("formatting %s's partition %s\n",hd->name,part->name);
            partition_format(part);
          }
        }
        part_idx++;
        part++;//下一个分区
      }
      dev_no++;
    }
    channel_no++;
  }
  sys_free(sb_buf);
}
