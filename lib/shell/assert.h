#ifndef __LIB_SHELL_ASSERT_H
#define __LIB_SHELL_ASSERT_H

#include "global.h"

void user_spin(char *filename,int line,const char *func,const char *condition);
#define panic(...) user_spin(__FILE__,__LINE__,__func__,__VA_ARGS__)

#ifdef NDEBUG
  #define assert(CONDITION) ((void)0)
#else
  #define assert(CONDITION)   \
    if (!(CONDITION)) {       \
      panic(#CONDITION);      \
    }
#endif
#endif
//assert()中填写判断条件,panin()填报错原因
