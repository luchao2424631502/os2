#ifndef __KERNEL_DEBUG_H
#define __KERNEL_DEBUG_H

void panic_spin(char*,int,const char*,const char*);

#define PANIC(...) panic_spin(__FILE__,__LINE__,__func__,__VA_ARGS__)

#ifdef NDEBUG
  #define ASSERT(CONDITION) ((void)0)
#else//#符号使得参数变为字符串常量
  #define ASSERT(CONDITION)\
  if (CONDITION) {} else {\
    PANIC(#CONDITION);\
  }
#endif

#define debug_print_int(str,num) \
  do{\
    put_str(str);\
    put_int(num);\
    put_str("\n");\
  } \
  while(0)

#endif
