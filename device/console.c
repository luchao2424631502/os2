#include "console.h"
#include "print.h"
#include "stdint.h"
#include "sync.h"
#include "thread.h"

/*控制台锁*/
static struct lock console_lock;

void console_init()
{
  lock_init(&console_lock);
}

/*获取终端*/
void console_acquire()
{
  lock_acquire(&console_lock);
}

/*释放终端*/
void console_release()
{
  lock_release(&console_lock);
}

/*终端打印字符串*/
void console_put_str(char *str)
{
  console_acquire();

  put_str(str);

  console_release();
}

/*终端打印字符*/
void console_put_char(uint8_t char_ascii)
{
  console_acquire();

  put_char(char_ascii);

  console_release();
}

/*终端输出16进制数字*/
void console_put_int(uint32_t num)
{
  console_acquire();

  put_int(num);

  console_release();
}

