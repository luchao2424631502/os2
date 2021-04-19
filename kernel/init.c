#include "init.h"
#include "print.h"
#include "interrupt.h"
#include "timer.h"
#include "thread.h"
#include "memory.h"
#include "console.h"
#include "keyboard.h"
#include "tss.h"

void init_all()
{
  put_str("[init_all]\n");
  idt_init(); //初始化中断
  // 3/29晚上测试mem_init()函数;
  mem_init();
  
  // 4/5 测试thread_init()
  thread_init();

  timer_init(); //初始化定时器

  // 4/8 测试console_init
  console_init();

  // 4/9 测试keyboard
  keyboard_init();

  // 4/10 测试tss 
  tss_init();
}
