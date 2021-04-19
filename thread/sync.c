#include "sync.h"
#include "list.h"
#include "global.h"
#include "debug.h"
#include "interrupt.h"

/*信号量初始化*/
void sema_init(struct semaphore *sema,uint8_t value)
{
  sema->value = value;
  list_init(&sema->waiters);
}

/*由二元信号量构成的锁*/
void lock_init(struct lock *lock)
{
  lock->holder = NULL;
  lock->holder_repeat_nr = 0;
  sema_init(&lock->semaphore,1);
}

/*进入临界区,想获得信号量资源*/
void sema_down(struct semaphore *sema)
{
  /*关中断,原子操作*/
  enum intr_status old_status = intr_disable();

  //信号量资源不够,当前线程是要被阻塞执行的,
  while (sema->value == 0)
  {
    //当前肯定不在信号量等待队列中
    ASSERT(!elem_find(&sema->waiters,&running_thread()->general_tag));
    if (elem_find(&sema->waiters,&running_thread()->general_tag))
    {
      PANIC("[PANIC] thread/sync.c/sema_down(): thread blocked has been in waiters_list\n");
    }

    //首次加入到semaphore的等待队列中
    list_append(&sema->waiters,&running_thread()->general_tag);
    //阻塞当前线程,使其等待信号量
    thread_block(TASK_BLOCKED);
  }

  //能执行到这里说明,线程恢复阻塞拿到了信号量
  sema->value--;
  ASSERT(sema->value == 0);//二元信号量实现的

  intr_set_status(old_status);
}

/*退出临界区,释放信号量资源*/
void sema_up(struct semaphore *sema)
{
  /*关中断,原子操作*/
  enum intr_status old_status = intr_disable();
  ASSERT(sema->value == 0);
  //信号量等待者队列不能为空,才能释放信号量资源
  if (!list_empty(&sema->waiters))
  {
    //由waiters队列节点标签拿到对应的task_struct
    struct task_struct *thread_blocked = elem2entry(struct task_struct,general_tag,list_pop(&sema->waiters));
    thread_unblock(thread_blocked);
  }

  sema->value++;
  ASSERT(sema->value == 1);

  intr_set_status(old_status);
}

//21/4/8: 来实现锁
/*获得锁*/
void lock_acquire(struct lock* lock)
{
  //当前运行线程有锁还要占有锁
  if (lock->holder != running_thread())
  {
    //临界区
    sema_down(&lock->semaphore);
    lock->holder = running_thread();//修改锁的占有者为当前运行的线程
    ASSERT(lock->holder_repeat_nr == 0);
    lock->holder_repeat_nr = 1;
  }
  //锁的占有者不是当前线程
  else 
  {
    lock->holder_repeat_nr++;
  }
}

/*释放锁*/
void lock_release(struct lock* lock)
{
  //来释放锁的肯定是当前正在运行的线程
  ASSERT(lock->holder == running_thread());
  //多重调用lock_acquire
  if (lock->holder_repeat_nr > 1)
  {
    lock->holder_repeat_nr--;
    return ;
  }
  //释放锁,所以当前nr=1
  ASSERT(lock->holder_repeat_nr == 1);

  lock->holder = NULL;
  lock->holder_repeat_nr = 0;
  //释放信号量
  sema_up(&lock->semaphore);
}
