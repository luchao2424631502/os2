#include "graphics.h"
#include "string.h"
#include "interrupt.h"
#include "io.h"

static void init_palette();
static void set_palette(int,int,unsigned char *);
static void desktop_init();

/*填充一个矩形区域
 * vram:vram 虚拟地址
 * xwidth: 需要填充矩形的宽度(x1-x0)
 * c:填充的颜色(index)
 * x0,y0:左上角地址
 * x1,y1:右下角地址
 * */
void boxfill8(uint8_t *vram,int xwidth,uint8_t c,
              int x0,int y0,int x1,int y1)
{
  for (int y=y0; y<=y1; y++)
  {
    for (int x=x0; x<=x1; x++)
    {
      vram[y*xwidth+x] = c;
    }
  }
}

/*桌面初始化*/
static void desktop_init()
{
  uint32_t xsize = Width_320;
  uint32_t ysize = Length_200;
  uint8_t *vram = (uint8_t *)VGA_START_V; 

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
}

void graphics_init()
{
  /*初始化调色板*/
  init_palette();
  /*桌面基本元素初始化*/
  desktop_init();

  // int vga_start = VGA_START_V;
  // uint8_t *p = (uint8_t *)vga_start;
  // boxfill8(p,320,COL8_008484,20,20,120,120); //亮红
  // boxfill8(p,320,COL8_848484,70,50,170,150); //亮绿
  // boxfill8(p,320,COL8_c6c6c6,120,80,220,180);//
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
