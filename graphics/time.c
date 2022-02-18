#include "graphics.h"
#include "interrupt.h"
#include "vramio.h"

#define TIMER_FLAGS_ALLOC 1 //已配置状态
#define TIMER_FLAGS_USING 2 //超时器运行中
/*
 *定义在 graphics.h中
 */
struct TIMERCTL timerctl;

struct TIMER *timer,*timer2,*timer3;
struct FIFO8 timerfifo,timerfifo2,timerfifo3;


/*
 * 从timerctl中分配一个timer
 */
struct TIMER *timer_alloc()
{
	for (int i=0; i<MAX_TIMER; i++)
	{
		if (timerctl.timers0[i].flags==0)
		{
			timerctl.timers0[i].flags = TIMER_FLAGS_ALLOC;
			return &timerctl.timers0[i];
		}
	}
	return 0;
}

/*
 * 释放一个timer
 */
void timer_free(struct TIMER *timer)
{
	timer->flags = 0;
	return ;
}

/*
 * 初始化timer
 */
void timer_init_g(struct TIMER *timer, struct FIFO8 *fifo, unsigned char data) 
{
	timer->fifo = fifo;
	timer->data = data;
	return ;
}

/*
 * 设置timer的超时时间
 * 时钟中断是1s发生100次,则timeout的单位是 10^(-2) s
 * timeout含义变为截至时间
 */
void timer_settime(struct TIMER *timer,unsigned int timeout)
{
	timer->timeout = timeout + timerctl.count;
	timer->flags = TIMER_FLAGS_USING;
	
	enum intr_status old_status = intr_disable();
	unsigned int i;
	for (i=0; i < timerctl.using; i++)
		if (timerctl.timers[i]->timeout >= timer->timeout)
			break;
	for (unsigned int j=timerctl.using; j>i; j--)
		timerctl.timers[j] = timerctl.timers[j-1];
	timerctl.using++;
	timerctl.timers[i] = timer;
	timerctl.next = timerctl.timers[0]->timeout;

	intr_set_status(old_status);

	return ;
}

/*
 * 2022-02-14:计时器每一次时钟中断都记一次时间,10ms产生一次时钟中断,则1s产生100次中断
 * 被intr_timer_handler(时钟中断处理函数)调用
 */
void intr_timer_handler_in() 
{
	timerctl.count++;
	//当前时刻 没有到达 下一个需要通知的时刻
	if (timerctl.count < timerctl.next)
	{
		return ;
	}

	unsigned int i=0;
	//超时时间由近到远
	for (; i<timerctl.using; i++)
	{
		if (timerctl.timers[i]->timeout > timerctl.count)
			break;
		//超时
		timerctl.timers[i]->flags = TIMER_FLAGS_ALLOC;
		fifo8_put(timerctl.timers[i]->fifo,timerctl.timers[i]->data);
	}
	timerctl.using -= i;
	//将未超时的timer移动到timers[]数组前面来
	for (unsigned int j=0; j<timerctl.using; j++)
		timerctl.timers[j] = timerctl.timers[i+j];	
	if (timerctl.using > 0)
		timerctl.next = timerctl.timers[0]->timeout;
	else 
		timerctl.next = 0xffffffff;

	return ;
}

/*
 * 被device下的timer_init()调用
 */
void init_timer_in_pit() 
{
	/* 2022-02-14
	 *1.因为在Pic中先关闭了时钟中断,此时已经设置完时钟,所以先在要重新开启时钟中断
	 *2.:在PIT(programable interval timer)初始化函数中初始化 计时器
	 */
	timerctl.count = 0;
	timerctl.next = 0xffffffff;
	timerctl.using = 0;
	for (int i=0; i<MAX_TIMER; i++)
		timerctl.timers0[i].flags = 0;
	return ;
}
