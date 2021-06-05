/*
 * 2021-4-28:
 * 此文件是 用户空间(ring 3进程)调用系统API的方法的实现:
 * 通过int 0x80所以封装int 0x80并且帮助传递参数
 * */
#include "syscall.h"

/*无参数的系统调用*/
#define _syscall0(NUMBER) ({    \
    int retval;                 \
    asm volatile (              \
        "int $0x80;"            \
        :"=a"(retval)           \
        :"a" (NUMBER)           \
        :"memory"               \
        );                      \
    retval;                     \
})

/*1个参数的系统调用 ebx*/
#define _syscall1(NUMBER,ARG1) ({\
    int retval;                  \
    asm volatile (               \
        "int $0x80;"             \
        :"=a"(retval)            \
        :"a"(NUMBER),"b"(ARG1)   \
        :"memory"                \
    );                           \
    retval;                      \
})

/*2个参数的系统调用 ebx,ecx*/
#define _syscall2(NUMBER,ARG1,ARG2) ({\
    int retval;                       \
    asm volatile (                    \
        "int $0x80;"                  \
        :"=a"(retval)                 \
        :"a"(NUMBER),"b"(ARG1),"c"(ARG2)\
        :"memory"                     \
    );                                \
    retval;                           \
})

/*3个参数的系统调用 ebx,ecx,edx*/
#define _syscall3(NUMBER,ARG1,ARG2,ARG3) ({\
    int retval;                            \
    asm volatile(                          \
        "int $0x80;"                       \
        :"=a"(retval)                      \
        :"a"(NUMBER),"b"(ARG1),"c"(ARG2),"d"(ARG3)\
        :"memory"                          \
    );                                     \
    retval;                                \
})

/*----系统调用用户接口:----*/
uint32_t getpid()
{
  return _syscall0(SYS_GETPID);
}

//5-19新的sys_write实现
uint32_t write(int32_t fd,const void *buf,uint32_t count)
{
  // return _syscall1(SYS_WRITE,str);
  return _syscall3(SYS_WRITE,fd,buf,count);
}

void *malloc(uint32_t size)
{
  return (void *)_syscall1(SYS_MALLOC,size);
}

void free(void *ptr)
{
  _syscall1(SYS_FREE,ptr);
}

pid_t fork()
{
  return _syscall0(SYS_FORK);
}

int32_t read(int32_t fd,void *buf,uint32_t count)
{
  return _syscall3(SYS_READ,fd,buf,count);
}

void putchar(char char_asci)
{
  _syscall1(SYS_PUTCHAR,char_asci);
}

void clear()
{
  _syscall0(SYS_CLEAR);
}

/*获取当前工作目录*/
char *getcwd(char *buf,uint32_t size)
{
  return (char *)_syscall2(SYS_GETCWD,buf,size);
}

/*打开pathname文件,标志是flag*/
int32_t open(char *pathname,uint8_t flag)
{
  return _syscall2(SYS_OPEN,pathname,flag);
}

/*关闭文件(描述符是fd)*/
int32_t close(int32_t fd)
{
  return _syscall1(SYS_CLOSE,fd);
}

/*设置文件偏移量*/
int32_t lseek(int32_t fd,int32_t offset,uint8_t whence)
{
  return _syscall3(SYS_LSEEK,fd,offset,whence);
}

/*删除文件*/
int32_t unlink(const char *pathname)
{
  return _syscall1(SYS_UNLINK,pathname);
}

/*创建目录*/
int32_t mkdir(const char *pathname)
{
  return _syscall1(SYS_MKDIR,pathname);
}

/*打开目录dir*/
struct dir *opendir(const char *name)
{
  return (struct dir *)_syscall1(SYS_OPENDIR,name);
}

/*关闭目录dir*/
int32_t closedir(struct dir *dir)
{
  return _syscall1(SYS_CLOSEDIR,dir);
}

/*删除目录dir*/
int32_t rmdir(const char *pathname)
{
  return _syscall1(SYS_RMDIR,pathname);
}

/*读取目录*/
struct dir_entry *readdir(struct dir *dir)
{
  return (struct dir_entry *)_syscall1(SYS_READDIR,dir);
}

/*目录指针rewind*/
void rewinddir(struct dir *dir)
{
  _syscall1(SYS_REWINDDIR,dir);
}

/*获取属性*/
int32_t stat(const char *path,struct stat *buf)
{
  return _syscall2(SYS_STAT,path,buf);
}

/*更改工作目录*/
int32_t chdir(const char *path)
{
  return _syscall1(SYS_CHDIR,path);
}

/*显示任务列表*/
void ps()
{
  _syscall0(SYS_PS);
}

int32_t execv(const char *pathname,char **argv)
{
  return _syscall2(SYS_EXECV,pathname,argv);
}

void exit(int32_t status)
{
  _syscall1(SYS_EXIT,status);
}

pid_t wait(int32_t *status)
{
  return _syscall1(SYS_WAIT,status);
}
