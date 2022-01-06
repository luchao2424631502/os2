#ifndef __GRAPHICS_INCLUDE_VRAMIO_H
#define __GRAPHICS_INCLUDE_VRAMIO_H

#include "stdint.h"

// void write_mem8(uint32_t addr,uint8_t data);

#define FLAGS_OVERRUN 0x001
#define FIFO8_BUF_SIZE  512 
struct FIFO8
{
  unsigned char buf[FIFO8_BUF_SIZE];
  int p,q,size,free,flags;
};

void fifo8_init(struct FIFO8 *fifo);
int fifo8_put(struct FIFO8 *fifo,unsigned char);
int fifo8_get(struct FIFO8 *);
int fifo8_status(struct FIFO8 *);
int fifo8_empty(struct FIFO8 *fifo);
int fifo8_full(struct FIFO8 *fifo);

#endif 
