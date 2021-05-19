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
