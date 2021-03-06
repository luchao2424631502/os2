#ifndef __FS_FS_H
#define __FS_FS_H

#include "stdint.h"
// #include "ide.h"

#define MAX_FILES_PER_PART  4096//分区支持的最多inode
#define BITS_PER_SECTOR     4096
#define SECTOR_SIZE         512
#define BLOCK_SIZE  SECTOR_SIZE

#define MAX_PATH_LEN  512       //路径最大长度

/*文件类型*/
enum file_types
{
  FT_UNKNOWN,   //不支持
  FT_REGULAR,   //普通文件
  FT_DIRECTORY  //目录
};

/*打开文件的选项*/
enum oflags
{
  O_RDONLY,   //read only
  O_WRONLY,   //write only
  O_RDWR  ,   //read write
  O_CREAT=4   //create file
};

/*文件读写位置偏移量*/
enum whence
{
  SEEK_SET = 1,
  SEEK_CUR,
  SEEK_END
};

/*记录查找文件过程中已经找到的上级路径,*/
struct path_search_record
{
  char searched_path[MAX_PATH_LEN];   //父路径
  struct dir *parent_dir;             //直接父目录
  enum file_types file_type;          //文件类型
};

/*文件属性*/
struct stat
{
  uint32_t st_ino;    //inode
  uint32_t st_size;   //文件大小
  enum file_types st_filetype;  //文件类型
};

extern struct partition *cur_part;
void filesys_init();
char *path_parse(char *,char *);
int32_t path_depth_cnt(char *);
int32_t sys_open(const char *,uint8_t);
int32_t sys_close(int32_t);
int32_t sys_write(int32_t,const void *,uint32_t);//代替userprog/syscall-init.c中的原来的sys_write()
int32_t sys_read(int32_t,void *,uint32_t);
int32_t sys_lseek(int32_t,int32_t,uint8_t);
int32_t sys_unlink(const char *);//删除文件
int32_t sys_mkdir(const char *);
struct dir *sys_opendir(const char *);
int32_t sys_closedir(struct dir *);
struct dir_entry *sys_readdir(struct dir *);
void sys_rewinddir(struct dir *);
int32_t sys_rmdir(const char *);
char *sys_getcwd(char *,uint32_t);
int32_t sys_chdir(const char *);
int32_t sys_stat(const char *,struct stat *);
void sys_putchar(char);
uint32_t fd_local2global(uint32_t);
void sys_help();
#endif
