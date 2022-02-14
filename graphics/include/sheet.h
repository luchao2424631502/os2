#ifndef _GRAPHICS_SHEET_H
#define _GRAPHICS_SHEET_H

//图层 32byte
struct SHEET 
{
  unsigned char *buf;   //buffer
  int bxsize,bysize;    //图层的大小
  int vx0,vy0;          //图层在320x200上的坐标
  int col_inv;          //透明颜色 color_and_invisible
  int height;           //图层的高度
  int flags;            //图层的设定
  struct SHEETCTL *ctl; //所属于的ctl (control)
};

#define MAX_SHEETS      256
#define MAP_SIZE        64000
//sheet的选项
#define SHEET_USE       1

//图层的管理结构 9232 byte
struct SHEETCTL
{
  unsigned char *vram;  //显存虚拟地址: 0xc00a0000 4byte
  unsigned char *map;   //屏幕每个像素所属于的图层
  int xsize,ysize;      //屏幕的大小 8byte
  int top;              //最上图层的高度 
  struct SHEET *sheets[MAX_SHEETS];   //给sheets0排序后用 4*256=1k
  struct SHEET sheets0[MAX_SHEETS];   //每个图层实际的存放  32*256=8k 
};

struct SHEETCTL *sheetctl_init(unsigned char *vram,int xsize,int ysize);
struct SHEET *sheet_alloc(struct SHEETCTL *ctl);
void sheet_updown(struct SHEET *sht,int height);
void sheet_setbuf(struct SHEET *sht,unsigned char *buf,int xsize,int ysize,int col_inv);
void sheet_refresh(struct SHEET *sht,int bx0,int by0,int bx1,int by1);
void sheet_refreshsub(struct SHEETCTL *ctl,int vx0,int vy0,int vx1,int vy1,int h0,int h1);
void sheet_refreshmap(struct SHEETCTL *ctl,int vx0,int vy0,int vx1,int vy1,int height);
void sheet_slide(struct SHEET *sht,int vx0,int vy0);
void sheet_free(struct SHEET *sht);
#endif