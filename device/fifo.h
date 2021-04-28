#ifndef __DEVICE_FIFO_H
#define __DEVICE_FIFO_H

#include "stdint.h"
/*
 * fifo产生的原因:
 * 因为ioqueue关联者生产者(中断键盘驱动_内核主线程),
 * 消费者(main中创建的内核线程)
 * 涉及到控制线程阻塞和使用调度器,
 * */

#define BUF_SIZE 256

/*类似ring buffer,但是没有同步机制*/
struct fifo
{
  uint8_t buf[BUF_SIZE];
  uint8_t next_r,next_w;
  uint8_t length;
}

/**/
uint8_t fifo_read();
uint8_t fifo_write();

#endif
