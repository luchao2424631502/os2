#ifndef __FS_FILE_H
#define __FS_FILE_H

#include "stdint.h"
#include "inode.h"
#include "ide.h"
#include "dir.h"
#include "global.h"

/*文件描述符背后对应的结构*/
struct file 
{
  uint32_t fd_pos;        //当前文件操作的偏移量
  uint32_t fd_flag;       //此次文件操作的类型:read write open
  struct inode *fd_inode; //打开的文件对应的inode
};

/*std io的fd*/
enum std_fd
{
  stdin_no,
  stdout_no,
  stderr_no
};

enum bitmap_type
{
  INODE_BITMAP,
  BLOCK_BITMAP
};

#define MAX_FILE_OPEN 32//系统可打开的最大文件树

extern struct file file_table[MAX_FILE_OPEN];
int32_t get_free_slot_in_global();
int32_t pcb_fd_install(int32_t ); 
int32_t inode_bitmap_alloc(struct partition *);
int32_t block_bitmap_alloc(struct partition *);
void bitmap_sync(struct partition *,uint32_t,uint8_t);
int32_t file_create(struct dir *,char *,uint8_t );
int32_t file_open(uint32_t,uint8_t);
int32_t file_close(struct file *);
int32_t file_write(struct file *,const void *,uint32_t);
int32_t file_read(struct file *,void *,uint32_t);
#endif
