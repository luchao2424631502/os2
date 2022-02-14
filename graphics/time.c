#include "graphics.h"
#include "interrupt.h"
#include "vramio.h"
/*
 *初始化在 device/timer.c下
 *定义在 graphics.h中
 */
struct TIMERCTL timerctl;
struct FIFO8 timerfifo;

/*
 * 初始化TIMERCTL 的timeout部分
 */
void settimer(unsigned int timeout, struct FIFO8 *fifo, unsigned char data) 
{
	//先关闭中断(因为时钟中断会判断timeout的值,所以先关闭中断
	enum intr_status old_status = intr_disable();

	timerctl.timeout = timeout;
	timerctl.fifo = fifo;
	timerctl.data = data;

	intr_set_status(old_status);
}

void init_timer_in_pit() 
{
	/* 2022-02-14
	 *1.因为在Pic中先关闭了时钟中断,此时已经设置完时钟,所以先在要重新开启时钟中断
	 *2.:在PIT(programable interval timer)初始化函数中初始化 计时器
	 */
	timerctl.count = 0;
	timerctl.timeout = 0;

	/*
	 * 
	 */
	fifo8_init(&timerfifo);
	settimer(1000,&timerfifo,1);
}

/*
 * 被intr_timer_handler(时钟中断处理函数)调用
 */
void intr_timer_handler_in() 
{
	/*
	 *2022-02-14:计时器每一次时钟中断都记一次时间,10ms产生一次时钟中断
	 *则1s增加100
	 */
	timerctl.count++;
	if (timerctl.timeout > 0)
	{
		timerctl.timeout--;
		if (timerctl.timeout == 0)
		{
			fifo8_put(timerctl.fifo,timerctl.data);
		}
	}
	return ;
}
