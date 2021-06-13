#include "graphics.h"
#include "string.h"
#include "interrupt.h"
#include "io.h"
#include "debug.h"

#include "font.h"
#include "paint.h"
#include "mouse.h"

#include "print.h"
#include "vramio.h"

extern uint8_t _binary_graphics_1_in_start[];
extern uint8_t _binary_graphics_1_in_end[];
extern uint8_t _binary_graphics_1_in_size[];

static void init_palette();
static void set_palette(int,int,unsigned char *);
static void desktop_init();
static void screen_init();

/*打算在内核中测试(实现)图形操作*/
void kernel_graphics()
{
}

void graphics_init()
{
  /*初始化调色板*/
  init_palette();


  /*桌面基本元素初始化*/
  desktop_init();
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

/*桌面初始化*/
static void desktop_init()
{
  /*拿到loader.s中填写的信息(填写成功)
   * */
  struct BOOT_INFO *boot_info = (struct BOOT_INFO*)0xc0000ff0;
  screen_init(boot_info->vram,boot_info->scrnx,boot_info->scrny);
}

static void screen_init(uint8_t *vram_,uint32_t x,uint32_t y)
{
  uint32_t xsize = x;
  uint32_t ysize = y;
  uint8_t *vram = vram_;
  
  // uint32_t xsize = Width_320;
  // uint32_t ysize = Length_200;
  // uint8_t *vram = (uint8_t *)VGA_START_V;

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
  putfont8(vram,xsize,0,0,COL8_000000,'A');
  putfont8(vram,xsize,8,0,COL8_840000,'B');
  putfont8(vram,xsize,16,0,COL8_840000,'C');
  putfont8(vram,xsize,24,0,COL8_000000,'1');
  
  putfont8_str(vram,xsize,0,16,7,"");
  putfont8_str(vram,xsize,16,16,3,"llc OS");


  /*显示鼠标*/
  char mcursor[256];
  /*根据鼠标图像得到 像素颜色数组*/
  init_mouse_cursor8(mcursor,BACKGROUND_COLOR);
  /*将鼠标显示出来*/
  putblock8(vram,xsize,16,16,0,64,mcursor,16);
}

