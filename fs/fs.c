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
#include "list.h"

//默认情况下操作的是哪一个分区
struct partition *cur_part;

/*在分区链表中找到part_name分区,并且赋值给cur_part*/
static bool mount_partition(struct list_elem *elem,int arg)
{
  char *part_name = (char *)arg;
  struct partition *part = elem2entry(struct partition,part_tag,elem);
  if (!strcmp(part->name,part_name))
  {
    cur_part = part;
    struct disk *hd = cur_part->my_disk;

    //超级块缓存
    struct super_block *sb_buf = (struct super_block*)sys_malloc(SECTOR_SIZE);

    //当前super_block的信息
    cur_part->sb = (struct super_block*)sys_malloc(sizeof(struct super_block));
    if (cur_part->sb == NULL)
    {
      PANIC("alloc memory failed!");
    }

    memset(sb_buf,0,SECTOR_SIZE);
    ide_read(hd,cur_part->start_lba+1,sb_buf,1);//将当前分区的superblock读入sb_buf中

    memcpy(cur_part->sb,sb_buf,sizeof(struct super_block));
    cur_part->block_bitmap.bits = (uint8_t *)sys_malloc(sb_buf->block_bitmap_sects * SECTOR_SIZE);
    if (cur_part->block_bitmap.bits == NULL)
    {
      PANIC("alloc memory failed!");
    }
    cur_part->block_bitmap.btmp_bytes_len = sb_buf->block_bitmap_sects * SECTOR_SIZE;

    //将原分区的block bitmap扇区读取到内存中来
    ide_read(hd,sb_buf->block_bitmap_lba,cur_part->block_bitmap.bits,sb_buf->block_bitmap_sects);

    /*inode bitmap读入内存*/
    cur_part->inode_bitmap.bits = (uint8_t *)sys_malloc(sb_buf->inode_bitmap_sects * SECTOR_SIZE);
    if (cur_part->inode_bitmap.bits == NULL)
    {
      PANIC("alloc memory failed!");
    }
    cur_part->inode_bitmap.btmp_bytes_len = sb_buf->inode_bitmap_sects * SECTOR_SIZE;
    ide_read(hd,sb_buf->inode_bitmap_lba,cur_part->inode_bitmap.bits,sb_buf->inode_bitmap_sects);

    list_init(&cur_part->open_inodes);
    printk("[mount] %s done!",part->name);

    return true;
  }
  return false;
}

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


/**/
static char *path_parse(char *pathname,char *name_store)
{
  if (pathname[0] == '/')
  {
    while (*(++pathname) == '/');
  }

  while (*pathname != '/' && *pathname != 0)
  {
    *name_store++ = *pathname++;
  }

  //到结尾了
  if (pathname[0] == 0)
  {
    return NULL;
  }

  return pathname;
}

/*路径深度*/
int32_t path_depth_cnt(char *pathname)
{
  ASSERT(pathname != NULL);
  char *p = pathname;
  char name[MAX_FILE_NAME_LEN];
  uint32_t depth = 0;

  p = path_parse(p,name);
  while (name[0])
  {
    depth++;
    memset(name,0,MAX_FILE_NAME_LEN);
    if (p)
    {
      p = path_parse(p,name);
    }
  }
  return depth;
}

/*给出路径pathname,搜索找到其inode号*/
static int search_file(const char *pathname,struct path_search_record *searched_record)
{
  //搜索根目录,根目录的当前目录,根目录的父目录,都直接填好search_record后返回
  if (!strcmp(pathname,"/") || !strcmp(pathname,"/.") || !strcmp(pathname,"/.."))
  {
    searched_record->parent_dir = &root_dir;
    searched_record->file_type = FT_DIRECTORY;
    searched_record->searched_path[0] = 0;
    return 0;
  }

  uint32_t path_len = strlen(pathname);
  ASSERT(pathname[0] == '/' && path_len > 1 && path_len < MAX_PATH_LEN);
  char *sub_path = (char *)pathname;
  struct dir *parent_dir = &root_dir;
  struct dir_entry dir_e;

  //路径各级的名称
  char name[MAX_FILE_NAME_LEN] = {0};

  searched_record->parent_dir = parent_dir;
  searched_record->file_type = FT_UNKNOWN;
  uint32_t parent_inode_no = 0;

  sub_path = path_parse(sub_path,name);
  while (name[0])
  {
    //曾搜索过的路径
    ASSERT(strlen(searched_record->searched_path) < 512);

    //字符串拼接,这一次拿到的文件名
    strcat(searched_record->searched_path,"/");
    strcat(searched_record->searched_path,name);

    //在目录dir下查找 本次 提取出来的当前目录的文件（子目录)
    if (search_dir_entry(cur_part,parent_dir,name,&dir_e))
    {
      memset(name,0,MAX_FILE_NAME_LEN);
      if (sub_path)
      {
        sub_path = path_parse(sub_path,name);
      }

      if (dir_e.f_type == FT_DIRECTORY) //name文件是dir
      {
        parent_inode_no = parent_dir->inode->i_no;
        dir_close(parent_dir);
        parent_dir = dir_open(cur_part,dir_e.i_no);//当前目录作为父目录,进行下一次查找
        searched_record->parent_dir = parent_dir;
        continue;
      }
      else if (dir_e.f_type == FT_REGULAR) //不一定找到了,可能是路径中的一个文件
      {
        searched_record->file_type = FT_REGULAR;
        return dir_e.i_no;
      }
    }
    else 
    {
      return -1;
    }
  }

  dir_close(searched_record->parent_dir);

  searched_record->parent_dir = dir_open(cur_part,parent_inode_no);
  searched_record->file_type = FT_DIRECTORY;
  return dir_e.i_no;
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

  char default_part[8] = "sdb1";
  //挂载sdb1分区
  list_traversal(&partition_list,mount_partition,(int)default_part);
}
