#ifndef __LIB_USER_SYSCALL_H
#define __LIB_USER_SYSCALL_H

#include "stdint.h"
#include "fs.h"
#include "thread.h"

/*根据内核将系统调用的实现顺序
 *来在用户空间定义api的index顺序*/
enum SYSCALL_NR{
  SYS_GETPID,
  SYS_WRITE,
  SYS_MALLOC,
  SYS_FREE,
  SYS_FORK,
  SYS_READ,
  SYS_PUTCHAR,
  SYS_CLEAR,
  /*5-23添加*/
  SYS_GETCWD,
  SYS_OPEN,
  SYS_CLOSE,
  SYS_LSEEK,
  SYS_UNLINK,
  SYS_MKDIR,
  SYS_OPENDIR,
  SYS_CLOSEDIR,
  SYS_CHDIR,
  SYS_RMDIR,
  SYS_READDIR,
  SYS_REWINDDIR,
  SYS_STAT,
  SYS_PS,
  SYS_EXECV,
  /*6-4添加*/
  SYS_EXIT,
  SYS_WAIT
};

/*用户空间api的函数声明*/
uint32_t getpid(void);
uint32_t write(int32_t,const void *,uint32_t);
void *malloc(uint32_t);
void free(void *);
int16_t fork();
int32_t read(int32_t,void *,uint32_t);
void putchar(char);
void clear();

char *getcwd(char *,uint32_t);
int32_t open(char *,uint8_t);
int32_t close(int32_t);
int32_t lseek(int32_t,int32_t,uint8_t);
int32_t unlink(const char *);
int32_t mkdir(const char *);
struct dir *opendir(const char *);
int32_t closedir(struct dir *);
int32_t rmdir(const char *);
struct dir_entry *readdir(struct dir *);
void rewinddir(struct dir *);
int32_t stat(const char *,struct stat*);
int32_t chdir(const char *);
void ps();
int32_t execv(const char *,char **);
void exit(int32_t);
pid_t wait(int32_t *);
#endif
