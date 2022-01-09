#include "graphics.h"
#include "string.h"
#include "interrupt.h"
#include "io.h"
#include "debug.h"
#include "global.h"
#include "font.h"
#include "mouse.h"
#include "memory.h"

#include "print.h"
#include "vramio.h"
#include "stdio.h"
#include "sheet.h"

#define SCREEN_BUF_SIZE 320*200
#define MOUSE_WIDTH 16
#define MOUSE_HEIGHT 16

extern uint8_t _binary_graphics_1_in_start[];
extern uint8_t _binary_graphics_1_in_end[];
extern uint8_t _binary_graphics_1_in_size[];

static void init_palette();
static void set_palette(int,int,unsigned char *);
static void screen_init();
static void boxfill8(unsigned char *vram,int xsize,unsigned char color,int x0,int y0,int x1,int y1);

/*k_graphics内核线程*/
void kernel_graphics(void *arg UNUSED)
{
  //将原来的init.c中初始化弄到后面来.
  graphics_init();

  while(1);
}

void graphics_init()
{
  /*初始化调色板*/
  init_palette();

  /*2022-1-9: 添加图层*/
  struct SHEETCTL* ctl;
  struct SHEET *sht_back,*sht_mouse;
  unsigned char *buf_back,buf_mouse[256];
  
  //给桌面的图层分配buffer
  buf_back = (unsigned char *)sys_malloc(SCREEN_BUF_SIZE);
  
  ctl = sheetctl_init((unsigned char *)VGA_START_V,Width_320,Length_200);
  sht_back = sheet_alloc(ctl);
  sht_mouse = sheet_alloc(ctl);
  sheet_setbuf(sht_back,buf_back,Width_320,Length_200,-1);  //col_inv=-1的透明色号可能表示不透明
  sheet_setbuf(sht_mouse,buf_mouse,MOUSE_WIDTH,MOUSE_HEIGHT,99);  //col_inv=99的透明色号 暂时不知道是什么意思

  
  /*桌面基本元素初始化*/
  struct BOOT_INFO *boot_info = (struct BOOT_INFO*)0xc0000ff0;
  screen_init(buf_back,boot_info->scrnx,boot_info->scrny);
  sheet_slide(ctl,sht_back,0,0);  //back桌面原点重定位

  /*根据鼠标图像得到 像素颜色数组*/
  init_mouse_cursor8(buf_mouse,99);
  mx = (320-16)/2;
  my = (200-28-16)/2;
  sheet_slide(ctl,sht_mouse,mx,my);
  sheet_updown(ctl,sht_back,0);
  sheet_updown(ctl,sht_mouse,1);

  /*
  // 图层测试(打印鼠标坐标)
  char buf[64];
  sprintf(buf,"(%d,%d)",mx,my);
  // putfont8_str(buf_back,320,0,0,7,buf);
  boxfill8(buf_back,320,0,0,0,100,20);
  sheet_refresh(ctl);
  */

  /*要初始化鼠标我在init.c中做了*/
  k_mouse(ctl,sht_mouse);
}

/*初始化调色板,支持16种颜色*/
static void init_palette()
{
  static unsigned char table_rgb[16*3] = {
    0x00,0x00,0x00, /*0.#000000 黑色*/
    0xff,0x00,0x00, /*1.#ff0000 亮红*/
    0x00,0xff,0x00, /*2.#00ff00 亮绿*/
    0xff,0xff,0x00, /*3.#ffff00 亮黄*/
    0x00,0x00,0xff, /*4.#0000ff 亮蓝*/
    0xff,0x00,0xff, /*5.#ff00ff 亮紫*/

    0x00,0xff,0xff, /*6.#00ffff 浅亮蓝*/
    0xff,0xff,0xff, /*7.#ffffff 白色*/
    0xc6,0xc6,0xc6, /*8.#c6c6c6 亮灰*/
    0x84,0x00,0x00, /*9.#840000 暗红*/
    0x00,0x84,0x00, /*10.#008400 暗绿*/
    0x84,0x84,0x00, /*11.#848400 暗黄*/

    0x00,0x00,0x84, /*12.#       暗蓝*/
    0x84,0x00,0x84, /*13.#       暗紫*/
    0x00,0x84,0x84, /*14.#       浅灰蓝*/
    0x84,0x84,0x84  /*15.#       暗灰*/
  };

  set_palette(0,15,table_rgb);
}

static void screen_init(uint8_t *vram_,uint32_t x,uint32_t y)
{
  uint32_t xsize = x;
  uint32_t ysize = y;
  uint8_t *vram = vram_;
  

  /*画横线和填充dock任务栏(矩形)*/
  boxfill8(vram,xsize,COL8_008484,0     ,0       ,xsize-1  ,ysize-29);//亮蓝填充上部分
  boxfill8(vram,xsize,COL8_c6c6c6,0     ,ysize-28,xsize-1  ,ysize-28);//下的一条亮灰线
  boxfill8(vram,xsize,COL8_ffffff,0     ,ysize-27,xsize-1  ,ysize-27);//下一行来一条白线
  boxfill8(vram,xsize,COL8_c6c6c6,0     ,ysize-26,xsize-1  ,ysize-1); //剩下的下面用亮灰色填满

  /*画左下的矩形*/
  boxfill8(vram,xsize,COL8_ffffff,3     ,ysize-24,59       ,ysize-24);//左矩的上白色边
  boxfill8(vram,xsize,COL8_ffffff,2     ,ysize-24,2        ,ysize-4); //左矩的左白色边
  boxfill8(vram,xsize,COL8_848484,3     ,ysize-4 ,59       ,ysize-4); //左矩的下暗灰边
  boxfill8(vram,xsize,COL8_848484,59    ,ysize-24,59       ,ysize-4); //左矩的右暗灰边
  boxfill8(vram,xsize,COL8_000000,2     ,ysize-3 ,59       ,ysize-3); //下暗灰边下+一条黑边
  boxfill8(vram,xsize,COL8_000000,60    ,ysize-24,60       ,ysize-3); //右暗灰边+一条黑边

  /*画右下的矩形*/
  boxfill8(vram,xsize,COL8_848484,xsize-47 ,ysize-24  ,xsize-4   ,ysize-24);//右矩上暗灰边
  boxfill8(vram,xsize,COL8_848484,xsize-47 ,ysize-23  ,xsize-47  ,ysize-4 );//右矩左暗灰边
  boxfill8(vram,xsize,COL8_ffffff,xsize-47 ,ysize-3   ,xsize-4   ,ysize-3 );//右矩下白边
  boxfill8(vram,xsize,COL8_ffffff,xsize-3  ,ysize-24  ,xsize-3   ,ysize-3 );//右矩右白边

  /*测试*/
  // putfont8(vram,xsize,0,0,COL8_000000,'A');
  // putfont8(vram,xsize,8,0,COL8_840000,'B');
  // putfont8(vram,xsize,16,0,COL8_840000,'C');
  // putfont8(vram,xsize,24,0,COL8_000000,'1');
  // putfont8_str(vram,xsize,0,16,7,"");
  // putfont8_str(vram,xsize,16,16,3,"llc OS");


}

/*设置vga的16个寄存器(调色板)*/
static void set_palette(int start,int end,unsigned char *rgb)
{
  //关中断
  enum intr_status old_status = intr_disable();
  
  //连续设置(调色板)寄存器
  outb(0x03c8,start);
  for (int i=start; i<=end; i++)
  {
    outb(0x03c9,rgb[0]/4);
    outb(0x03c9,rgb[1]/4);
    outb(0x03c9,rgb[2]/4);
    rgb += 3;
  }

  //恢复中断
  intr_set_status(old_status);
}

//
static void boxfill8(unsigned char *vram,int xsize,unsigned char color,int x0,int y0,int x1,int y1)
{
  for (int y=y0; y<=y1; y++)
    for (int x=x0; x<=x1; x++)
      vram[y * xsize + x] = color;
  return ;
}
