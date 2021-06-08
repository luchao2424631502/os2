#include "stdio-kernel.h"
#include "stdio.h"
#include "console.h"
#include "global.h"

#define va_start(args,first_fix) args = (va_list)&first_fix
#define va_end(args) args = NULL
#define K 1024

void printk(const char *format,...)
{
  va_list args;
  va_start(args,format);
  char buf[K] = {0};
  vsprintf(buf,format,args);
  va_end(args);
  
  //通过vsprintf写入buf后 用console_put_str()打印出来
  console_put_str(buf);
}

