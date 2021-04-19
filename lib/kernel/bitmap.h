#ifndef __LIB_KERNEL_BITMAP_H
#define __LIB_KERNEL_BITMAP_H

#include "global.h"

#define BITMAP_MASK 1

struct bitmap {
  uint32_t btmp_bytes_len;
  uint8_t *bits;
};

void bitmap_init(struct bitmap*);
bool bitmap_scan_test(struct bitmap *,uint32_t);
int bitmap_scan(struct bitmap *,uint32_t ); //scan bitmap,得到连续为0的bit的下标索引
void bitmap_set(struct bitmap *,uint32_t,int8_t); //将bitmap的bit_index的bit设置为value
#endif
