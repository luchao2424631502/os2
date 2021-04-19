#ifndef __THREAD_SYNC_H
#define __THREAD_SYNC_H

#include "list.h"
#include "stdint.h"
#include "thread.h"

/*二元信号量*/
struct semaphore
{
  uint8_t value;
  struct list waiters; //
};

/*锁*/
struct lock
{
  struct task_struct *holder;//锁的持有者
  struct semaphore semaphore;//用2元信号量实现锁
  uint32_t holder_repeat_nr; //锁持有者申请锁的次数
};

void sema_init(struct semaphore *,uint8_t);
void sema_down(struct semaphore *);
void sema_up(struct semaphore *);

void lock_init(struct lock*);
void lock_acquire(struct lock*);
void lock_release(struct lock*);

#endif
