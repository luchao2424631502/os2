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
#include "debug.h"
#include "string.h"

//接收鼠标传来的数据
struct ioqueue mouse_buf;
//接收鼠标传来的数据的结构
struct MOUSE_DEC
{
  uint8_t buf[3],phase;
  int x,y,btn;
};
//鼠标中心的坐标
int mx,my;
//鼠标像素数组
char mcursor[256];

static void intr_mouse_handler();
static void mouse_chip_init();
static void wait_keysta_ready();
static void enable_mouse();
static int mouse_decode(struct MOUSE_DEC *,unsigned char );

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

/*内核线程:用来处理鼠标中断发出来的数据*/
void k_mouse(void *arg UNUSED)
{
  putfont8_str((uint8_t *)VGA_START_V,320,160,84,13,"I'm k_mouse");

  struct MOUSE_DEC mdec;
  mdec.phase = 0;

  uint8_t mouse_dbuf[3];
  memset(mouse_dbuf,0,3);
  uint8_t data;

  while (1)
  {
    enum intr_status old_status = intr_disable();
    if (!ioq_empty(&mouse_buf))
      data = ioq_getchar(&mouse_buf);
    intr_set_status(old_status);

    if (mouse_decode(&mdec,data) != 0)
    {
      char buf[50];
      memset(buf,0,50);
      sprintf(buf,"[lcr %d %d]",mdec.x,mdec.y);
      if((mdec.btn & 0x01) != 0)
      {
        buf[1] = 'L';
      }
      if ((mdec.btn & 0x02) != 0)
      {
        buf[3] = 'R';
      }
      if ((mdec.btn & 0x04) != 0)
      {
        buf[2] = 'C';
      }
      boxfill8((uint8_t *)VGA_START_V,320,14,32,16,32+15*8 - 1,31);
      putfont8_str((uint8_t *)VGA_START_V,320,32,16,0,buf);

      /*鼠标指针的移动*/
      boxfill8((uint8_t *)VGA_START_V,320,14,mx,my,mx+15,my+15);//隐藏鼠标

      mx += mdec.x;
      my += mdec.y;
      if (mx < 0)
      {
        mx = 0;
      }
      if (my < 0)
      {
        my = 0;
      }
      if (mx > 320 - 16)
      {
        mx = 320-16;
      }
      if (my > 200 - 16)
      {
        my = 200 - 16;
      }
      memset(buf,0,sizeof(buf));
      sprintf(buf,"(%d,%d)",mx,my);
      //隐藏坐标
      boxfill8((uint8_t *)VGA_START_V,320,14,0,0,79,15);
      //显示坐标
      putfont8_str((uint8_t *)VGA_START_V,320,0,0,7,buf);
      //显示鼠标
      putblock8((uint8_t *)VGA_START_V,320,16,16,mx,my,mcursor,16);
    }
  }

  while (1);
  PANIC("k_mouse: error");
}

static int mouse_decode(struct MOUSE_DEC *mdec,unsigned char data)
{
  if (mdec->phase == 0)
  {
    //等待进入鼠标的0xfa状态
    if (data == 250)
    {
      mdec->phase = 1;
    }
    return 0;
  }
  //拿到第一个字节
  if (mdec->phase == 1)
  {
    if ((data & 0xc8) == 0x08)
    {
      mdec->buf[0] = data;
      mdec->phase = 2;
    }
    return 0;
  }
  //拿到第2个字节
  if (mdec->phase == 2)
  {
    mdec->buf[1] = data; 
    mdec->phase = 3;
    return 0;
  }
  //拿到第3个字节
  if (mdec->phase == 3) 
  {
    mdec->buf[2] = data;
    mdec->phase = 1;

    mdec->btn = mdec->buf[0] & 0x07;//只取低3bit
    mdec->x = mdec->buf[1];
    mdec->y = mdec->buf[2];
    if ((mdec->buf[0] & 0x10) != 0)
    {
      mdec->x |= 0xffffff00;
    }
    if ((mdec->buf[0] & 0x20) != 0)
    {
      mdec->y |= 0xffffff00;
    }

    mdec->y = -mdec->y;

    return 1;
  }
  return -1;
}
