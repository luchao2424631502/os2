#include "stdio.h"
#include "global.h"
#include "string.h"
#include "syscall.h"
#include "file.h"

#define HEX 16
#define DEC 10
#define K  1024

#define va_start(ap,v) ap=(va_list)&v   //ap指向第一个固定参数
#define va_arg(ap,t) *((t*)(ap += 4))     //ap指向下一个参数并返回值
#define va_end(ap) ap=NULL              //清除ap

static void itoa(uint32_t,char **,uint8_t);

/*将int 转化为 ascii*/
static void itoa(uint32_t value,char **buf_ptr_addr,uint8_t base)
{
  uint32_t mod     = (value % base);
  uint32_t i = (value / base);
  if (i)
  {
    itoa(i,buf_ptr_addr,base);
  }
  //余数是[0,10)
  if (mod < 10)
  {
    *((*buf_ptr_addr)++) = mod + '0';
  }
  //余数是[A,F]
  else 
  {
    *((*buf_ptr_addr)++) = mod + 'A' - 10;
  }
}

/*
 * 支持%x %d %c %s
 * */
uint32_t vsprintf(char *buf,const char *format,va_list ap)
{
  char *buf_ptr = buf;
  const char *index_ptr = format;
  char index_char = *index_ptr;
  int32_t arg_int;
  char *arg_str;
  while (index_char)
  {
    //不是格式化输出符号
    if (index_char != '%')
    {
      /*直接将字符写入转换后的Buf中`*/
      *(buf_ptr++) = index_char;
      index_char = *(++index_ptr);
      continue;
    }
    //得到%后面的字符%d %x %s %c
    index_char = *(++index_ptr);

    switch(index_char)
    {
      case 's':
        arg_str = va_arg(ap,char *);
        /*字符串复制,但是buf_ptr值没有改变,所以后面增加他的长度*/
        strcpy(buf_ptr,arg_str);
        buf_ptr += strlen(arg_str);
        index_char = *(++index_ptr);
        break;
      case 'c':
        *(buf_ptr++) = va_arg(ap,char);
        index_char = *(++index_ptr);
        break;
      case 'd':
        arg_int = va_arg(ap,int);
        /*处理负数,变成正数的补码,0-(负数补码)=正数补码*/
        if (arg_int < 0)
        {
          arg_int = 0 - arg_int;
          *buf_ptr++ = '-';
        }
        itoa(arg_int,&buf_ptr,DEC);
        index_char = *(++index_ptr);
        break;
      case 'x':
        arg_int = va_arg(ap,int);
        itoa(arg_int,&buf_ptr,HEX);
        index_char = *(++index_ptr);
        break;
      default:
        break;
    }
  }
  return strlen(buf);
}

/*sprintf*/
uint32_t sprintf(char *buf,const char *format,...)
{
  va_list args;

  va_start(args,format);
  uint32_t retval = vsprintf(buf,format,args);
  va_end(args);

  return retval;
}

/*格式化输出*/
uint32_t printf(const char *format,...)
{
  va_list args;
  va_start(args,format);
  char buf[K] = {0};
  vsprintf(buf,format,args);
  va_end(args);
  return write(stdout_no,buf,strlen(buf));
}

