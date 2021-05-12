#ifndef __FS_FS_H
#define __FS_FS_H

#include "stdint.h"
#include "ide.h"

#define MAX_FILES_PER_PART  4096//分区支持的最多inode
#define BITS_PER_SECTOR     4096
#define SECTOR_SIZE         512
#define BLOCK_SIZE  SECTOR_SIZE

enum file_types
{
  FT_UNKNOWN,   //不支持
  FT_REGULAR,   //普通文件
  FT_DIRECTORY  //目录
};

void filesys_init();
#endif
