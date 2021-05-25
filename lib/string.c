#include "string.h"
// #include "global.h"
// #include "debug.h"
#include "assert.h"

/*内存赋值*/
void memset(void* dst_,uint8_t value,uint32_t count)
{
  // ASSERT(dst_ != NULL);
  assert(dst_ != NULL);
  uint8_t *dst = (uint8_t*)dst_;
  while(count-- > 0)
    *dst++ = value;
}

/*内存copy*/
void memcpy(void *dst_,const void *src_,uint32_t count)
{
  // ASSERT(dst_ != NULL && src_ != NULL);
  assert(dst_ != NULL && src_ != NULL);
  uint8_t *dst = dst_;
  const uint8_t *src = src_;
  while(count-- > 0)
    *dst++ = *src++;
}

/*内存数据段比较*/
int memcmp(const void* a_,const void *b_,uint32_t count)
{
  const char *a = a_;
  const char *b = b_;
  //2021/4/18 修改
  // ASSERT(a!=NULL || b!=NULL);
  assert(a!=NULL || b!=NULL);
  while(count-- > 0)
  {
    if (*a != *b)
    {
      return *a > *b ? 1 : -1;
    }
    a++;
    b++;
  }
  return 0;
}

/*字符串复制*/
char* strcpy(char* dst_,const char* src_)
{
  // ASSERT(dst_ != NULL && src_ != NULL);
  assert(dst_ != NULL && src_ != NULL);
  char *r = dst_;
  while((*dst_++ = *src_++)){}
  return r;
}

/*测量字符串长度*/
uint32_t strlen(const char* str)
{
  // ASSERT(str != NULL);
  assert(str != NULL);
  const char *p = str;
  while(*p++){}
  return (p-str-1);
}

/*比较2个字符串*/
int8_t strcmp(const char* a,const char* b)
{
  // ASSERT(a != NULL && b != NULL);
  assert(a != NULL && b != NULL);
  while(*a != 0 && *a==*b)
  {
    a++;
    b++;
  }
  return *a<*b?-1:*a>*b;
}

/*从前到后 在str中查找首次出现ch字符的索引*/
char *strchr(const char* str,const uint8_t ch)
{
  // ASSERT(str != NULL);
  assert(str != NULL);
  while(*str != 0)
  {
    if (*str == ch)
    {
      return (char*)str;
    }
    str++;
  }
  return NULL;
}

/*从后往前 在str中查找首次出现ch字符的索引*/
char *strrchr(const char* str,const uint8_t ch)
{
  // ASSERT(str != NULL);
  assert(str != NULL);
  const char* lastchar = NULL;
  while(*str != 0)
  {
    if (*str == ch)
    {
      lastchar = str;
    }
    str++;
  }
  return (char*)lastchar;
}

/*字符串拼接*/
char *strcat(char *dst,const char *src)
{
  // ASSERT(dst != NULL && src != NULL);
  assert(dst != NULL && src != NULL);
  char *str = dst;
  while(*str++){}
  --str;
  while((*str++ = *src++)){}
  return dst;
}

/*在str中查找字符ch出现的次数*/
uint32_t strchrs(const char* str,uint8_t ch)
{
  // ASSERT(str != NULL);
  assert(str != NULL);
  uint32_t ch_cnt = 0;
  const char *p = str;
  while (*p != 0)
  {
    if (*p == ch)
    {
      ch_cnt++;
    }
    p++;
  }
  return ch_cnt;
}
