#include "ioqueue.h"
#include "interrupt.h"
#include "global.h"
#include "debug.h"

/*初始化ioqueue*/
void ioqueue_init(struct ioqueue *ioq)
{
  lock_init(&ioq->lock);
  ioq->producter = ioq->consumer = NULL;
  ioq->head = ioq->tail = 0;
}

/*环形(取模)缓冲区*/
static int32_t next_pos(int32_t pos)
{
  return (pos + 1) % BUF_SIZE;
}

/*判断ring buffer是否满*/
bool ioq_full(struct ioqueue *ioq)
{
  ASSERT(intr_get_status() == INTR_OFF);
  return next_pos(ioq->head) == ioq->tail;
}

/*判断ring buffer是否为空*/
bool ioq_empty(struct ioqueue *ioq)
{
  ASSERT(intr_get_status() == INTR_OFF);
  return ioq->head == ioq->tail;
}

/*使当前生产者或消费者在ring buffer上等待*/
static void ioq_wait(struct task_struct **waiter)
{
  ASSERT(*waiter == NULL && waiter != NULL);
  *waiter = running_thread();
  thread_block(TASK_BLOCKED);
}

/*唤醒waiter*/
static void wakeup(struct task_struct **waiter)
{
  ASSERT(*waiter != NULL);
  thread_unblock(*waiter);
  *waiter = NULL;
}

/*
 * 单核下的 生产者消费者模型只有1个生产者和1个消费者,
 * 所以生产和消费时不需要互斥,
 * 只有防止过度生产和过度消费就行了
*/

/*消费者:拿走char*/
char ioq_getchar(struct ioqueue *ioq)
{
  ASSERT(intr_get_status() == INTR_OFF);

  //buffer为空,当前线程不能在消费下去
  while (ioq_empty(ioq))
  {
    lock_acquire(&ioq->lock);
    ioq_wait(&ioq->consumer);
    lock_release(&ioq->lock);
  }

  char ch = ioq->buffer[ioq->tail];   //取出字符
  ioq->tail = next_pos(ioq->tail);    //移动游标 

  /* 生产者把buffer生成满了,就会自我阻塞,
   * 如果当前消费者消费了,并且ioq->producter!=NULL,
   * 说明当前生产者被阻塞,那么将生产者唤醒,可以继续生产下去
   * */
  if (ioq->producter != NULL)
  {
    wakeup(&ioq->producter);
  }

  return ch;
}

/*生产者:向ring buffer中添加char*/
void ioq_putchar(struct ioqueue *ioq,char ch)
{
  ASSERT(intr_get_status() == INTR_OFF);

  //buffer满了,肯定当前线程不能继续生产下去
  while (ioq_full(ioq))
  {
    lock_acquire(&ioq->lock);
    ioq_wait(&ioq->producter);
    lock_release(&ioq->lock);
  }

  ioq->buffer[ioq->head] = ch;
  ioq->head = next_pos(ioq->head);

  /* 消费者因为buffer为空消费不了,所以会自我阻塞,
   * 当前生产者已经生产了一个,所以消费者可以消费,
   * ioq->consumer!=NULL,说明消费者被阻塞,那么唤醒消费者开始消费
   * */
  if (ioq->consumer != NULL)
  {
    wakeup(&ioq->consumer);
  }
}

uint32_t ioq_length(struct ioqueue *ioq)
{
  uint32_t len = 0;
  //head是生产者处理的,tail是消费者处理的
  if (ioq->head >= ioq->tail)
  {
    len = ioq->head - ioq->tail;
  }
  else
  {
    len = BUF_SIZE - (ioq->tail - ioq->head);
  }
  return len;
}
