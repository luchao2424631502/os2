#ifndef __FS_FS_H
#define __FS_FS_H

#include "stdint.h"
#include "ide.h"

#define MAX_FILES_PER_PART  4096//分区支持的最多inode
#define BITS_PER_SECTOR     4096
#define SECTOR_SIZE         512
#define BLOCK_SIZE  SECTOR_SIZE
#define MAX_PATH_LEN  512       //路径最大长度

enum file_types
{
  FT_UNKNOWN,   //不支持
  FT_REGULAR,   //普通文件
  FT_DIRECTORY  //目录
};

//打开文件的选项
enum oflags
{
  O_RDONLY,   //read only
  O_WRONLY,   //write only
  O_RDWR  ,   //read write
  O_CREAT=4   //create file
};

struct path_search_record
{
  char searched_path[MAX_PATH_LEN];   //父路径
  struct dir *parent_dir;             //直接父目录
  enum file_types file_type;          //文件类型
};

extern struct partition *cur_part;
void filesys_init();
int32_t path_depth_cnt(char *);
int32_t sys_open(const char *,uint8_t);
int32_t sys_close(int32_t);
#endif
