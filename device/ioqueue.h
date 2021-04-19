#ifndef __DEVICE_IOQUEUE_H
#define __DEVICE_IOQUEUE_H

#include "stdint.h"
#include "thread.h"
#include "sync.h"

#define BUF_SIZE 64 

/*ring buffer*/
struct ioqueue
{
  struct lock lock;
  struct task_struct *producter; //生产者
  struct task_struct *consumer;  //消费者
  
  char buffer[BUF_SIZE];
  int32_t head;
  int32_t tail;
};

void ioqueue_init(struct ioqueue*);
bool ioq_full(struct ioqueue*);
bool ioq_empty(struct ioqueue*);

char ioq_getchar(struct ioqueue *);     //从ringbuffer中得到char
void ioq_putchar(struct ioqueue *,char);//向ringbuffer中插入char

#endif
