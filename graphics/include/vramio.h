#ifndef __GRAPHICS_INCLUDE_VRAMIO_H
#define __GRAPHICS_INCLUDE_VRAMIO_H

#include "stdint.h"
#define FLAGS_OVERRUN 0x001

void write_mem8(uint32_t addr,uint8_t data);

struct FIFO8
{
  unsigned char *buf;
  int p,q,size,free,flags;
};

void fifo8_init(struct FIFO8 *,int,unsigned char *);
int fifo8_put(struct FIFO8 *fifo,unsigned char);
int fifo8_get(struct FIFO8 *);
int fifo8_status(struct FIFO8 *);


#endif 
