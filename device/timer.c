#include "timer.h"
#include "io.h"
#include "print.h"
#include "thread.h"
#include "interrupt.h"
#include "debug.h"


#define IRQ0_FREQUENCY 100 //1s中100次时钟中断
#define INPUT_FREQUENCY 1193180
#define COUNTER0_VALUE INPUT_FREQUENCY/IRQ0_FREQUENCY
#define COUNTER0_PORT 0x40
#define COUNTER0_NO 0
#define COUNTER0_MODE 2
#define READ_WRITE_LATCH 3
#define PIT_CONTROL_PORT 0x43

//时钟中断处理发生次数
uint32_t ticks;

static void frequency_set(uint8_t counter_port,
    uint8_t counter_no,
    uint8_t rwl,
    uint8_t counter_mode,
    uint16_t counter_value)
{
  outb(PIT_CONTROL_PORT,(uint8_t)(counter_no<<6 | rwl << 4 | counter_mode << 1));
  outb(counter_port,(uint8_t)counter_value);
  outb(counter_port,(uint8_t)counter_value >> 8);
}

/*时钟中断处理函数,(通过时钟来进行调度)*/
static void intr_timer_handler()
{
  struct task_struct *cur_thread = running_thread();
  ASSERT(cur_thread->stack_magic == 0x20010522);

  cur_thread->elapsed_ticks++; //线程经过的时钟中断调度次数
  ticks++;

  if (cur_thread->ticks == 0)
  {
    //调度器调度其它线程
    schedule();
  }
  else //时间片还有时间可用
  {
    cur_thread->ticks--;
  }
}

//初始化定时器8253
void timer_init()
{
  put_str("[timer_init] start\n");
  frequency_set(COUNTER0_PORT,COUNTER0_NO,READ_WRITE_LATCH,COUNTER0_MODE,COUNTER0_VALUE);
  /*注册时钟中断处理函数 interrupt.c/register_handler() */
  register_handler(0x20,intr_timer_handler);
  put_str("[timer_init] done\n");
}
