#ifndef __LIB_IO_H
#define __LIB_IO_H

#include "stdint.h"

//向端口写一个字节的数据
static inline void outb(uint16_t port,uint8_t data)
{
  //outb %al,%dx
  asm volatile ("outb %b0,%w1"
      :
      :"a"(data),"Nd"(port)
      );
}

//从一个端口读一个字节的数据
static inline uint8_t inb(uint16_t port)
{
  uint8_t data;
  //inb %dx,%al
  asm volatile ("inb %w1,%b0"
      :"=a"(data)
      :"Nd"(port)
      );
  return data;
}

//将addr处的cnt个字节写入端口port
static inline void outsw(uint16_t port,const void *addr,uint32_t word_cnt)
{
  // rep outsw将ds:esi的word写入port端口
  asm volatile ("cld;"
      "rep outsw;"
      :"+S"(addr),"+c"(word_cnt)
      :"d"(port)
      );
}

//从端口port读word_cnt字节到addr处,
static inline void insw(uint16_t port,void *addr,uint32_t word_cnt)
{
  asm volatile ("cld;"
      "rep insw"
      :"+D"(addr),"+c"(word_cnt)
      :"d"(port)
      :"memory"
      );
}

#endif
