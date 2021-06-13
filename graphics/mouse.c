#include "mouse.h"
#include "color256.h"
#include "print.h"
#include "interrupt.h"
#include "graphics.h"
#include "font.h"
#include "paint.h"
#include "io.h"
#include "stdio.h"
#include "vramio.h"
#include "ioqueue.h"

//接收鼠标传来的数据
struct ioqueue mouse_buf;

static void intr_mouse_handler();
static void mouse_chip_init();
static void wait_keysta_ready();
static void enable_mouse();

//鼠标指针大小是 16*16,生成的mouse数组每个char表示的是像素颜色
void init_mouse_cursor8(char *mouse,char bc)
{
  static char cursor[16][16] = {
		"**************..",
		"*OOOOOOOOOOO*...",
		"*OOOOOOOOOO*....",
		"*OOOOOOOOO*.....",
		"*OOOOOOOO*......",
		"*OOOOOOO*.......",
		"*OOOOOOO*.......",
		"*OOOOOOOO*......",
		"*OOOO**OOO*.....",
		"*OOO*..*OOO*....",
		"*OO*....*OOO*...",
		"*O*......*OOO*..",
		"**........*OOO*.",
		"*..........*OOO*",
		"............*OO*",
		".............***"
  };

  for (int y=0; y<16; y++)
  {
    for (int x=0; x<16; x++)
    {
      if (cursor[y][x] == '*')
        mouse[y*16 + x] = COL8_000000;
      else if (cursor[y][x] == 'O')
        mouse[y*16 + x] = COL8_ffffff;
      else 
        mouse[y*16 + x] = bc;
    }
  }
}

//给定座标和想显示的方块大小和像素数组
void putblock8(uint8_t *vram,int vxsize,int pxsize,int pysize,
    int px0,int py0,char *buf,int bxsize)
{
  for (int y=0; y<pysize; y++)
  {
    for (int x=0; x<pxsize; x++)
    {
      vram[((py0 + y) * vxsize) + (px0 + x)] = buf[(y * bxsize) + x];
    }
  }
}

// 2021/4/25: ps/2鼠标中断注册 
void mouse_init()
{
  put_str("[mouse_init] start\n");

  /*2021-6-11:在mouse中断开启前,将mouse buf初始化好*/
  ioqueue_init(&mouse_buf);
  
  //添加鼠标中断处理
  register_handler(0x2c,intr_mouse_handler);

  //首先对鼠标中的芯片进行初始化
  mouse_chip_init();

  put_str("[mouse_init] done\n");
}

/*鼠标中断*/
static void intr_mouse_handler()
{
  struct BOOT_INFO *bootinfo = (struct BOOT_INFO*)(0xc0000ff0);

  //鼠标中断debug测试
  // boxfill8(bootinfo->vram,bootinfo->scrnx,COL8_000000,0,0,32*8-1,15);
  // putfont8_str(bootinfo->vram,bootinfo->scrnx,0,0,COL8_ffff00,"IRQ 0x2c PS/2 mouse");

  /*2021-6-11继续*/
  uint8_t data = inb(PORT_KEYDAT);
  boxfill8((uint8_t *)0xc00a0000,320,14,160,100,160+24,100+48);
  putfont8_int((uint8_t *)0xc00a0000,320,160,100,0,data);

  if (!ioq_full(&mouse_buf))
  {
    ioq_putchar(&mouse_buf,data);
  }
}

static void wait_keysta_ready()
{
  /*读取端口0x64检测鼠标电路的状态,如果第2bit=0,则鼠标
   *可接受设置
   * */
  for (;;)
  {
    if ((inb(PORT_KEYSTA) & KEYSTA_SEND_NOTREADY) == 0)
      break;
  }
}

/**/
static void mouse_chip_init()
{
  /*通过键盘端口来开启鼠标芯片*/
  wait_keysta_ready();
  //向0x64端口返回可写信号(0x60))
  outb(PORT_KEYCMD,KEYCMD_WRITE_MODE);
  wait_keysta_ready();
  //向0x60端口发送0x47字节,(鼠标产生的数据可以通过键盘端口0x60读取
  outb(PORT_KEYDAT,KBC_MODE);

  enable_mouse();
}

static void enable_mouse()
{
  wait_keysta_ready();
  /*向鼠标发送数据前,先向端口发送0xd4,
   * 然后向0x60端口写入的数据都会发送给鼠标*/
  outb(PORT_KEYCMD,KEYCMD_SENDTO_MOUSE);
  wait_keysta_ready();
  /*向0x60端口写入0xf4,鼠标被激活,立刻向cpu发出中断*/
  outb(PORT_KEYDAT,MOUSECMD_ENABLE);
}
