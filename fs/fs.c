#include "fs.h"
#include "file.h"
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
#include "console.h"
#include "thread.h"
#include "ioqueue.h"
#include "keyboard.h"
#include "console.h"

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
    printk("[mount] %s done!\n",part->name);

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

/*将路径的根部的名称解析出来*/
char *path_parse(char *pathname,char *name_store)
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

/*open文件,失败返回-1*/
int32_t sys_open(const char *pathname,uint8_t flags)
{
  //对于目录文件需使用dir_open())
  if (pathname[strlen(pathname)-1] == '/')
  {
    printk("fs/fs.c sys_open() can't open a directory %s\n",pathname);
    return -1;
  }

  ASSERT(flags <= 7);
  int32_t fd = -1;

  struct path_search_record searched_record;
  memset(&searched_record,0,sizeof(struct path_search_record));

  uint32_t pathname_depth = path_depth_cnt((char*)pathname);

  //判断文件是否存在
  int inode_no = search_file(pathname,&searched_record);
  bool found = inode_no != -1 ? true : false;

  if (searched_record.file_type == FT_DIRECTORY)
  {
    printk("fs/fs.c sys_open():can't open a directory with open(),used opendir() to instead\n");
    dir_close(searched_record.parent_dir);
    return -1;
  }

  uint32_t path_searched_depth = path_depth_cnt(searched_record.searched_path);

  //判断是否访问了所有目录,如果处于中间目录则失败
  if (pathname_depth != path_searched_depth)
  {
    printk("fs/fs.c sys_open():cannot access %s: Not a directory, subpath %s is't exist\n",pathname,searched_record.searched_path);
    dir_close(searched_record.parent_dir);
    return -1;
  }

  //没有找到文件并且不是创建文件
  if (!found && !(flags & O_CREAT))
  {
    printk("fs/fs.c sys_open(): in path %s, file %s is't exist\n",searched_record.searched_path,(strrchr(searched_record.searched_path,'/') + 1));
    dir_close(searched_record.parent_dir);
    return -1;
  }
  //文件存在并且要创建
  else if (found && flags & O_CREAT)
  {
    printk("fs/fs.c sys_open(): %s has already exist!\n",pathname);
    dir_close(searched_record.parent_dir);
    return -1;
  }

  //好像目前就实现了文件创建,并没有实现文件打开
  switch (flags & O_CREAT)
  {
    case O_CREAT:
      printk("fs/fs.c sys_open(): creating file\n");
      fd = file_create(searched_record.parent_dir,(strrchr(pathname,'/') + 1),flags);
      dir_close(searched_record.parent_dir);
      break;
    /*除了O_CREAT以外的O_RDWR,O_WRONLY,O_RDONLY*/
    default:
      fd = file_open(inode_no,flags);
  }

  return fd;
}

/*将文件描述符转化为fd_table下标*/
static uint32_t fd_local2global(uint32_t local_fd)
{
  struct task_struct *cur = running_thread();
  int32_t global_fd = cur->fd_table[local_fd];
  ASSERT(global_fd >= 0 && global_fd < MAX_FILE_OPEN);//系统最多可以打开32个文件
  return (uint32_t)global_fd; 
}

/*关闭fd指向的文件,失败返回-1,成功返回0*/
int32_t sys_close(int32_t fd)
{
  int32_t ret = -1;
  if (fd > 2)//除去stdin(0) stdout(1) stderr(2)
  {
    uint32_t local_fd = fd_local2global(fd);
    ret = file_close(&file_table[local_fd]);
    running_thread()->fd_table[fd] = -1;//指向的fd_table的下标为0,
  }
  return ret;
}

/* 将buf中的连续count字节写入文件描述符fd
 * 底层是file_write(),*/
int32_t sys_write(int32_t fd,const void *buf,uint32_t count)
{
  if (fd < 0)
  {
    printk("fs/fs.c sys_write(): fd error\n");
    return -1;
  }

  //向屏幕输出,
  if (fd == stdout_no)
  {
    char tmp_buf[1024] = {0};
    memcpy(tmp_buf,buf,count);
    console_put_str(tmp_buf);
    return count;
  }

  //向文件写入
  uint32_t global_fd = fd_local2global(fd);//将进程中fd转化为file_table[]下标 
  struct file *wr_file = &file_table[global_fd];
  //根据进程打开文件时的性质判断是否是写
  if (wr_file->fd_flag & O_WRONLY || wr_file->fd_flag & O_RDWR)
  {
    uint32_t bytes_written = file_write(wr_file,buf,count);
    return bytes_written;
  }
  else 
  {
    console_put_str("fs/fs.c sys_write(): not allowed to write file without flag O_RDWR or O_WRONLY\n");
    return -1;
  }
}

/*从fd中读取count个字节到buf中,正常返回读取到的字节,失败返回-1*/
int32_t sys_read(int32_t fd,void *buf,uint32_t count)
{
  ASSERT(buf != NULL);
  int32_t ret = -1;
  if (fd < 0 || fd == stdout_no || fd == stderr_no)
  {
    printk("fs/fs.c sys_read(): fd error\n");
  }
  else if (fd == stdin_no)
  {
    char *buffer = buf;
    uint32_t bytes_read = 0;
    while (bytes_read < count)
    {
      /*在keyboard.c中向ioqueue中加入添加的字符(生产),这里来消费*/
      *buffer = ioq_getchar(&kbd_buf); 
      bytes_read++;
      buffer++;
    }
    ret = (bytes_read == 0 ? -1 : (int32_t)bytes_read);
  }
  else//普通情况 
  {
    uint32_t global_fd = fd_local2global(fd);
    ret = file_read(&file_table[global_fd],buf,count);
  }
  return ret;
}

/**/
int32_t sys_lseek(int32_t fd,int32_t offset,uint8_t whence)
{
  if (fd < 0)
  {
    printk("fs/fs.c sys_lseek(): fd error\n");
    return -1;
  }

  ASSERT(whence > 0 && whence < 4);
  uint32_t global_fd = fd_local2global(fd);
  struct file *pf = &file_table[global_fd];
  int32_t new_pos = 0;
  int32_t file_size = (int32_t)pf->fd_inode->i_size;
  
  switch(whence)
  {
    case SEEK_SET:    //相对于文件开头
      new_pos = offset;
      break;
    case SEEK_CUR:    //相对于文件当前打开的指针
      new_pos = (int32_t)pf->fd_pos + offset;
      break;
    case SEEK_END:    //相对于file_size
      new_pos = file_size + offset;
      break;
  }
  if (new_pos < 0 || new_pos > (file_size - 1))
    return -1;
  pf->fd_pos = new_pos;
  return pf->fd_pos;
}

/*删除文件,失败返回-1,成功返回0*/
int32_t sys_unlink(const char *pathname)
{
  ASSERT(strlen(pathname) < MAX_PATH_LEN);

  struct path_search_record searched_record;
  memset(&searched_record,0,sizeof(struct path_search_record));
  int inode_no = search_file(pathname,&searched_record);
  ASSERT(inode_no != 0);
  //没有找到文件
  if (inode_no == -1)
  {
    printk("fs/fs.c sys_unlink(): file %s not found!\n",pathname);
    dir_close(searched_record.parent_dir);
    return -1;
  }

  if (searched_record.file_type == FT_DIRECTORY)
  {
    printk("fs/fs.c sys_unlink(): can't delete a directory with unlink(),use rmdir() to instead\n");
    dir_close(searched_record.parent_dir);
    return -1;
  }

  //通过file_table[]检查文件是否打开
  uint32_t file_idx = 0;
  while (file_idx < MAX_FILE_OPEN)
  {
    //找到了pathname文件对应的file_idx,说明文件被打开了
    if (file_table[file_idx].fd_inode != NULL && (uint32_t)inode_no == file_table[file_idx].fd_inode->i_no)
      break;
    file_idx++;
  }

  if (file_idx < MAX_FILE_OPEN)
  {
    dir_close(searched_record.parent_dir);
    printk("fs/fs.c sys_unlink(): file %s is in use,not allow to delete!\n",pathname);
    return -1;
  }
  ASSERT(file_idx == MAX_FILE_OPEN);//没有被打开(不在内存中)

  void *io_buf = sys_malloc(SECTOR_SIZE + SECTOR_SIZE);
  if (io_buf == NULL)
  {
    dir_close(searched_record.parent_dir);
    printk("fs/fs.c sys_unlink(): sys_malloc() malloc for io_buf failed\n");
    return -1;
  }

  struct dir *parent_dir = searched_record.parent_dir;
  //删除目录项
  delete_dir_entry(cur_part,parent_dir,inode_no,io_buf);
  //释放inode struct以及inode数据块 
  inode_release(cur_part,inode_no);
  sys_free(io_buf);
  dir_close(searched_record.parent_dir);
  return 0;
}

/*创建目录,成功返回0,失败返回-1*/
int32_t sys_mkdir(const char *pathname)
{
  uint8_t rollback_step = 0;
  void *io_buf = sys_malloc(SECTOR_SIZE * 2);
  if (io_buf == NULL)
  {
    printk("fs/fs.c sys_mkdir(): sys_malloc for io_buf failed\n");
    return -1;
  }

  struct path_search_record searched_record;
  memset(&searched_record,0,sizeof(struct path_search_record));
  int inode_no = -1;
  inode_no = search_file(pathname,&searched_record);
  if (inode_no != -1)//说明找到相同的文件或者目录
  {
    printk("fs/fs.c sys_mkdir(): file or directory %s exist!\n",pathname);
    rollback_step = 1;
    goto rollback;
  }
  else//没有找到同名的文件或目录 
  {
    //判断路径是否完全存在
    uint32_t pathname_depth = path_depth_cnt((char *)pathname);
    uint32_t path_searched_depth = path_depth_cnt(searched_record.searched_path); 

    //没有访问到全部路径
    if (pathname_depth != path_searched_depth)
    {
      printk("fs/fs.c sys_mkdir(): cannot access %s, subpath %s is't exist\n",pathname,searched_record.searched_path);
      rollback_step = 1;
      goto rollback;
    }
  }

  struct dir *parent_dir = searched_record.parent_dir;
  char *dirname = strrchr(searched_record.searched_path,'/') + 1;

  inode_no = inode_bitmap_alloc(cur_part);//申请一个inode号
  if (inode_no == -1)
  {
    printk("fs/fs.c sys_mkdir(): allocate inode failed\n");
    rollback_step = 1;
    goto rollback;
  }

  struct inode new_dir_inode;
  inode_init(inode_no,&new_dir_inode);//初始化inode

  uint32_t block_bitmap_idx = 0;
  int32_t block_lba = -1;
  block_lba = block_bitmap_alloc(cur_part);//申请一个块
  if (block_lba == -1)
  {
    printk("fs/fs.c sys_mkdir(): block_bitmap_alloc for create directory failed\n");
    rollback_step = 2;
    goto rollback;
  }
  //写入目录项 . ..
  new_dir_inode.i_sectors[0] = block_lba;
  //将分配的块 同步到 硬盘中
  block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
  ASSERT(block_bitmap_idx != 0);
  bitmap_sync(cur_part,block_bitmap_idx,BLOCK_BITMAP);

  //将目录项'.' 和'..'写入目录的数据区
  memset(io_buf,0,SECTOR_SIZE * 2);
  struct dir_entry *p_de = (struct dir_entry *)io_buf;

  memcpy(p_de->filename,".",1);
  p_de->i_no = inode_no;
  p_de->f_type = FT_DIRECTORY;

  p_de++;

  memcpy(p_de->filename,"..",2);
  p_de->i_no = parent_dir->inode->i_no;//inode指向父目录的inode
  p_de->f_type = FT_DIRECTORY;
  //写入硬盘
  ide_write(cur_part->my_disk,new_dir_inode.i_sectors[0],io_buf,1);

  //给inode struct赋值
  new_dir_inode.i_size = 2 * cur_part->sb->dir_entry_size;

  /*在父目录中添加目录项*/
  struct dir_entry new_dir_entry;
  memset(&new_dir_entry,0,sizeof(struct dir_entry));
  create_dir_entry(dirname,inode_no,FT_DIRECTORY,&new_dir_entry);
  memset(io_buf,0,SECTOR_SIZE * 2);
  //将目录向写入父目录文件
  if (!sync_dir_entry(parent_dir,&new_dir_entry,io_buf))
  {
    printk("fs/fs.c sys_mkdir(): sync_dir_entry to disk failed\n");
    rollback_step = 2;
    goto rollback;
  }

  //父目录的inode同步到硬盘
  memset(io_buf,0,SECTOR_SIZE * 2);
  inode_sync(cur_part,parent_dir->inode,io_buf);

  //新创建的目录的inode同步到硬盘
  memset(io_buf,0,SECTOR_SIZE * 2);
  inode_sync(cur_part,&new_dir_inode,io_buf);

  //将inode bitmap同步到硬盘中
  bitmap_sync(cur_part,inode_no,INODE_BITMAP);

  sys_free(io_buf);
  dir_close(searched_record.parent_dir);
  return 0;

rollback:
  switch (rollback_step)
  {
    case 2:
      bitmap_set(&cur_part->inode_bitmap,inode_no,0);
    case 1:
      dir_close(searched_record.parent_dir);
      break;
  }
  sys_free(io_buf);
  return -1;
}

/*打开目录,失败返回NULL*/
struct dir *sys_opendir(const char *name)
{
  ASSERT(strlen(name) < MAX_PATH_LEN);
  if (name[0] == '/' && (name[1] == 0 || name[0] == '.'))//这里有问题.
  {
    return &root_dir;
  }

  //检查要打开的目录是否存在
  struct path_search_record searched_record;
  memset(&searched_record,0,sizeof(struct path_search_record));
  int inode_no = search_file(name,&searched_record);

  struct dir *ret = NULL;
  if (inode_no == -1)//打开的目录不存在
  {
    printk("fs/fs.c sys_opendir(): In %s,sub path %s not exist\n",name,searched_record.searched_path);
  }
  else 
  {
    if (searched_record.file_type == FT_REGULAR)
    {
      printk("fs/fs.c sys_opendir(): %s is regular file\n",name);
    }
    else if (searched_record.file_type == FT_DIRECTORY)
    {
      ret = dir_open(cur_part,inode_no);
    }
  }

  //关闭父目录
  dir_close(searched_record.parent_dir);
  return ret;
}

/*关闭目录dir,成功返回0,失败返回-1*/
int32_t sys_closedir(struct dir *dir)
{
  int32_t ret = -1;
  if (dir != NULL)
  {
    dir_close(dir);
    ret = 0;
  }
  return ret;
}

/*读取目录dir的1个目录项,失败返回NULL*/
struct dir_entry *sys_readdir(struct dir *dir)
{
  ASSERT(dir != NULL);
  return dir_read(dir);
}

/*将dir->dir_pos置0*/
void sys_rewinddir(struct dir *dir)
{
  dir->dir_pos = 0;
}

/*删除空目录,success=0,失败返回-1*/
int32_t sys_rmdir(const char *pathname)
{
  //搜索待删除的目录项是否存在
  struct path_search_record searched_record;
  memset(&searched_record,0,sizeof(struct path_search_record));
  int inode_no = search_file(pathname,&searched_record);
  ASSERT(inode_no != 0);
  int retval = -1;
  if (inode_no == -1)//没有找到此目录项
  {
    printk("fs/fs.c sys_rmdir(): In %s, sub path %s not exist\n",pathname,searched_record.searched_path);
  }
  else 
  {
    if (searched_record.file_type == FT_REGULAR)
    {
      printk("%s is regular file\n",pathname);
    }
    else 
    {
      struct dir *dir = dir_open(cur_part,inode_no);
      if (!dir_is_empty(dir))//判断此目录是否非空
      {
        printk("dir %s is not empty, it is not allowed to delete a nonempty directory!\n",pathname);
      }
      else 
      {
        //删除目录,父目录由searched_record.parent_dir得来
        if (!dir_remove(searched_record.parent_dir,dir))//成功返回0
        {
          retval = 0;
        }
      }
      dir_close(dir);
    }
  }
  dir_close(searched_record.parent_dir);
  return retval;
}

/*给出child inode no,得到父目录的inode no*/
static uint32_t get_parent_dir_inode_nr(uint32_t child_inode_nr,void *io_buf)
{
  //打开子目录的inode struct 
  struct inode *child_dir_inode = inode_open(cur_part,child_inode_nr);
  // . 和 .. 目录肯定在第1个扇区
  uint32_t block_lba = child_dir_inode->i_sectors[0];
  ASSERT(block_lba >= cur_part->sb->data_start_lba);
  inode_close(child_dir_inode);
  ide_read(cur_part->my_disk,block_lba,io_buf,1);
  struct dir_entry *dir_e = (struct dir_entry *)io_buf;
  ASSERT(dir_e[1].i_no < 4096 && dir_e[1].f_type == FT_DIRECTORY);
  return dir_e[1].i_no;
}

/*在inode为inode_nr的目录中查找inode为c_inode_nr的子目录名字
 *  失败返回-1
 * */
static int get_child_dir_name(uint32_t p_inode_nr,uint32_t c_inode_nr,char *path,void *io_buf)
{
  struct inode *parent_dir_inode = inode_open(cur_part,p_inode_nr);
  uint8_t block_idx = 0;
  uint32_t all_blocks[140] = {0},block_cnt = 12;
  while (block_idx < 12)
  {
    all_blocks[block_idx] = parent_dir_inode->i_sectors[block_idx];
    block_idx++;
  }

  //一级间接表存在
  if (parent_dir_inode->i_sectors[12])
  {
    ide_read(cur_part->my_disk,parent_dir_inode->i_sectors[12],all_blocks+12,1);
    block_cnt = 140;
  }
  //关闭父目录的inode
  inode_close(parent_dir_inode);

  struct dir_entry *dir_e = (struct dir_entry *)io_buf;
  uint32_t dir_entry_size = cur_part->sb->dir_entry_size;//常量
  uint32_t dir_entrys_per_sec = SECTOR_SIZE / dir_entry_size;//常量
  block_idx = 0;
  //遍历所有的扇区(块),寻找c_inode_nr
  while (block_idx < block_cnt)
  {
    if (all_blocks[block_idx])//当前块存在
    {
      ide_read(cur_part->my_disk,all_blocks[block_idx],io_buf,1);
      uint8_t dir_e_idx = 0;
      //遍历扇区的每一个目录项
      while (dir_e_idx < dir_entrys_per_sec)
      {
        //找到了
        if ((dir_e + dir_e_idx)->i_no == c_inode_nr)
        {
          strcat(path,"/");
          strcat(path,(dir_e + dir_e_idx)->filename);
          return 0;
        }
        dir_e_idx++;
      }
    }
    block_idx++;
  }
  return -1;//说明没找到
}

/*得到当前工作目录.如果buf==null,此函数自行分配,由用户释放,(但是目前假设buf!=NULL*/
char *sys_getcwd(char *buf,uint32_t size)
{
  ASSERT(buf != NULL);
  void *io_buf = sys_malloc(SECTOR_SIZE);
  if (io_buf == NULL)
  {
    return NULL;
  }

  struct task_struct *cur_thread = running_thread();
  int32_t parent_inode_nr = 0;
  int32_t child_inode_nr = cur_thread->cwd_inode_nr;
  ASSERT(child_inode_nr >= 0 && child_inode_nr < 4096);
  if (child_inode_nr == 0)//进程当前工作目录是根目录
  {
    buf[0] = '/';
    buf[1] = 0;
    return buf;
  }

  memset(buf,0,size);
  char full_path_reverse[MAX_PATH_LEN] = {0};

  //将当前目录向上查找直到找到根目录为止
  while ((child_inode_nr))
  {
    parent_inode_nr = get_parent_dir_inode_nr(child_inode_nr,io_buf);
    if (get_child_dir_name(parent_inode_nr,child_inode_nr,full_path_reverse,io_buf) == -1)
    {
      sys_free(io_buf);
      return NULL;
    }
    child_inode_nr = parent_inode_nr;
  }
  ASSERT(strlen(full_path_reverse) <= size);

  char *last_slash;
  while ((last_slash = strrchr(full_path_reverse,'/')))
  {
    uint16_t len = strlen(buf);
    strcpy(buf + len,last_slash);
    //1.作为strcpy的边界,2.使下一个/作为strrchr()的第一个出现的/
    *last_slash = 0;
  }
  sys_free(io_buf);
  return buf;
}

//更改当前工作目录为path,成功返回0,失败返回-1
int32_t sys_chdir(const char *path)
{
  int32_t ret = -1;
  //先判断path是否存在
  struct path_search_record searched_record;
  memset(&searched_record,0,sizeof(struct path_search_record));
  int inode_no = search_file(path,&searched_record);
  if (inode_no != -1)//存在
  {
    //文件类型是目录
    if (searched_record.file_type == FT_DIRECTORY)
    {
      /*核心:证明path存在,然后直接修改当前进程的cwd_inode_nr*/
      running_thread()->cwd_inode_nr = inode_no;
      ret = 0;
    }
    //其它类型
    else 
    {
      printk("fs/fs.c sys_chdir(): %s is regular file or other\n",path);
    }
  }
  dir_close(searched_record.parent_dir);
  return ret;
}

/**/
int32_t sys_stat(const char *path,struct stat *buf)
{
  //查看根目录的信息
  if (!strcmp(path,"/") || !strcmp(path,"/.") || !strcmp(path,"/.."))
  {
    buf->st_filetype = FT_DIRECTORY;
    buf->st_ino = 0;
    buf->st_size = root_dir.inode->i_size;
  }

  int32_t ret = -1;
  struct path_search_record searched_record;
  memset(&searched_record,0,sizeof(struct path_search_record));
  int inode_no = search_file(path,&searched_record);
  //存在
  if (inode_no != -1)
  {
    struct inode *obj_inode = inode_open(cur_part,inode_no);
    buf->st_size = obj_inode->i_size;
    inode_close(obj_inode);
    buf->st_filetype = searched_record.file_type;
    buf->st_ino = inode_no;
    ret = 0;
  }
  else 
  {
    printk("fs/fs.c sys_stat(): %s not found\n",path);
  }
  dir_close(searched_record.parent_dir);
  return ret;
}

//向屏幕输出一个字符
void sys_putchar(char char_asci)
{
  console_put_char(char_asci);
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
          {
            printk("%s has filesystem\n",part->name);

            /*测试代码:拿到sdb1的数据起始lba
            partition_format(part);
            if (!strcmp(part->name,"sdb1"))
            {
              while (1);
            }
            */
          }
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

  /*将当前分区的根目录打开*/
  open_root_dir(cur_part);

  //初始化全局文件表
  uint32_t fd_idx = 0;
  while (fd_idx < MAX_FILE_OPEN)
  {
    file_table[fd_idx++].fd_inode = NULL;
  }
}
