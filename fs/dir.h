#ifndef __FS_DIR_H
#define __FS_DIR_H

#include "stdint.h"
#include "inode.h"
#include "ide.h"
#include "global.h"
#include "fs.h"

#define MAX_FILE_NAME_LEN 16

/*文件(目录文件)的结构:存储多个目录项*/
struct dir
{
  struct inode *inode;    //文件的inode
  uint32_t dir_pos;       //目录项的index
  uint8_t  dir_buf[512];  //buf
};

/*目录项 结构*/
struct dir_entry
{
  char filename[MAX_FILE_NAME_LEN];   //文件名
  uint32_t i_no;                      //指向的文件的Inode
  enum file_types f_type;             //指向的文件的类型
};

extern struct dir root_dir;     //根目录
void open_root_dir(struct partition *);
struct dir *dir_open(struct partition *,uint32_t);
void dir_close(struct dir *);
bool search_dir_entry(struct partition *,struct dir *,const char *,struct dir_entry*);
void create_dir_entry(char *,uint32_t,uint8_t,struct dir_entry *);
bool sync_dir_entry(struct dir *,struct dir_entry *,void *);
bool delete_dir_entry(struct partition *,struct dir *,uint32_t,void *);
struct dir_entry* dir_read(struct dir*);
bool dir_is_empty(struct dir *);
int32_t dir_remove(struct dir *,struct dir *);
#endif
