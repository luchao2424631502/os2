#include "init.h"
#include "print.h"
#include "interrupt.h"
#include "timer.h"
#include "thread.h"
#include "memory.h"
#include "console.h"
#include "keyboard.h"
#include "tss.h"
// <<<<<<< HEAD
#include "graphics.h"
#include "mouse.h"
// =======
#include "syscall-init.h"
#include "ide.h"
#include "fs.h"
// >>>>>>> master

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

// <<<<<<< HEAD
  // 4/25 graphics下mouse初始化(添加中断处理程序
  mouse_init();

  // 4/22 graphics部分
  graphics_init();

// =======
  // 4-29: 添加syscall
  syscall_init();

  {
    //开中断
    intr_enable();
    // 5-7: 添加硬盘初始化
    ide_init();
  }

  // 5-12添加文件系统初始化
  filesys_init();
// >>>>>>> master
}
