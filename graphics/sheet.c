#include "sheet.h"
#include "graphics.h"
#include "memory.h"
#include "stdio.h"
#include "assert.h"

//给sheetctl块分配 9232个字节
struct SHEETCTL *sheetctl_init(unsigned char *vram,int xsize,int ysize)
{
  struct SHEETCTL *ctl;

  /*
  enum pool_flags pflag = PF_KERNEL;
  ctl = (struct SHEETCTL *)malloc_page(pflag,SHEETCTL_PAGE);
  */
  ctl = (struct SHEETCTL *)sys_malloc(SHEETCTL_SIZE);

  if (!ctl)
  {
    panic("graphics/sheet.c struct SHEETCTL initialize fail");
  }

  ctl->vram = vram;
  ctl->xsize = xsize;
  ctl->ysize = ysize;
  //看起来图层最底高度是-1
  ctl->top = -1;
  for (int i=0; i<MAX_SHEETS; i++)
    ctl->sheets0[i].flags = 0;
  return ctl;
}

//从sheetctl中拿到一个sheet
struct SHEET *sheet_alloc(struct SHEETCTL *ctl)
{
  struct SHEET *sht;
  for (int i=0; i<MAX_SHEETS; i++)
  {
    if (ctl->sheets0[i].flags == 0)
    {
      sht = &ctl->sheets0[i];
      sht->flags = SHEET_USE;
      sht->height = -1;         //和sheetctl的top一个意思,高度-1表示当前不显示
      return sht;
    }
  }
  //所以的sheet都被用了,(90%不可能)
  return NULL;
}

//设置sheet参数,buffer和x,ysize
void sheet_setbuf(struct SHEET *sht,unsigned char *buf,
                  int xsize,int ysize,int col_inv)
{
  sht->buf = buf;
  sht->bxsize = xsize;
  sht->bysize = ysize;
  sht->col_inv = col_inv;
  return ;
}

/*
 * 设定sht的高度
 * 
*/
void sheet_updown(struct SHEETCTL *ctl,struct SHEET *sht,int height)
{
  if (height > ctl->top + 1)
  {
    height = ctl->top + 1;
  }

  if (height < -1)
  {
    height = -1;
  }
  int old = sht->height;
  sht->height = height;

  if (height < old)
  {
    //移动old到height之间的sht
    if (height >= 0)
    {
      for (int h=old; h>height; h--)
      {
        ctl->sheets[h] = ctl->sheets[h-1];
        ctl->sheets[h]->height = h;
      }
      //此时h是等于height的
      ctl->sheets[height] = sht;
    }
    //隐藏 height == -1
    else 
    {
      //不是最顶层的图层,抽走这一层,将上面的图层移动下来
      if (old < ctl->top)
      {
        for (int h=old; h<ctl->top; h++)
        {
          ctl->sheets[h] = ctl->sheets[h+1];
          ctl->sheets[h]->height = h;
        }
      }
      ctl->top--;
    }
    //变化完高度(区域是不变的),刷新指定区域的所有图层
    sheet_refreshsub(ctl,sht->vx0,sht->vy0,
                      sht->vx0+sht->bxsize,sht->vy0+sht->bysize);
  }
  //上移
  else if (height > old)
  {
    if (old >= 0)
    {
      for (int h=old; h<height; h++)
      {
        ctl->sheets[h] = ctl->sheets[h+1];
        ctl->sheets[h]->height = h;
      }
      ctl->sheets[height] = sht;
    }
    else //由隐藏变为显示
    {
      for (int h=ctl->top; h>=height; h--)
      {
        ctl->sheets[h+1] = ctl->sheets[h];
        ctl->sheets[h+1]->height = h+1;
      }
      ctl->sheets[height] = sht;
      ctl->top++;
    }
    sheet_refreshsub(ctl,sht->vx0,sht->vy0,
                      sht->vx0+sht->bxsize,sht->vy0+sht->bysize);
  }
  return ;
}

/*
 * (bx0,by0)
 * (bx1,by1)
 * 给出的坐标是相对的,刷新对于这个图层的相对区域
 * 
 * */
void sheet_refresh(struct SHEETCTL *ctl,struct SHEET *sht,int bx0,int by0,int bx1,int by1)
{
  if (sht->height >= 0)
  {
    sheet_refreshsub(ctl,sht->vx0+bx0,sht->vy0+by0,
                  sht->vx0+bx1,sht->vy0+by1);
  }
  return ;
}

/*
 * (vx0,vy0):屏幕上的左上坐标
 * (vx1,vy1):屏幕上的右下坐标
 * 给出的屏幕的两个坐标是绝对的,是要刷新这个绝对区域的所有图层
 * */
void sheet_refreshsub(struct SHEETCTL *ctl,int vx0,int vy0,int vx1,int vy1)
{
  unsigned char *buf;
  unsigned char *vram = ctl->vram;
  struct SHEET *sht;
  //对于每个图层来说 根据绝对区域裁剪出对于此图层的相对区域,然后拿到图层的像素,然后刷新
  for (int h=0; h<=ctl->top; h++)
  {
    sht = ctl->sheets[h];
    buf = sht->buf;

    //想要刷新的范围的绝对x坐标 - 图层的绝对x坐标 = 对于图层来说的相对bx坐标
    int bx0 = vx0 - sht->vx0;
    int by0 = vy0 - sht->vy0;
    int bx1 = vx1 - sht->vx0;
    int by1 = vy1 - sht->vy0;

    //可能要刷新的范围(有部分)在这个图层的外部,
    //在外部的就不用刷新了
    if (bx0 < 0) { bx0 = 0; }
    if (by0 < 0) { by0 = 0; }
    if (bx1 > sht->bxsize) { bx1 = sht->bxsize; }
    if (by1 > sht->bysize) { by1 = sht->bysize; }

    //只遍历要刷新的相对区域
    for (int by = by0; by<by1; by++)
    {
      int vy = sht->vy0 + by;
      for (int bx = bx0; bx<bx1; bx++)
      {
        int vx = sht->vx0 + bx;
        unsigned char ch = buf[by*sht->bxsize + bx];
        if (ch != sht->col_inv)
        {
          vram[vy*ctl->xsize + vx] = ch;
        }
      }
    }
  }
}

//移动图层(改变图层的原点的坐标就行)
void sheet_slide(struct SHEETCTL *ctl,struct SHEET *sht,int vx0,int vy0)
{
  int old_vx0 = sht->vx0;
  int old_vy0 = sht->vy0;
  sht->vx0 = vx0;
  sht->vy0 = vy0;
  //正在显示则移动图层后要重新绘制
  if (sht->height >= 0)
  {
    //刷新原来的图层
    sheet_refreshsub(ctl,old_vx0,old_vy0,
                      old_vx0+sht->bxsize,old_vy0+sht->bysize);
    //刷新移动后的图层
    sheet_refreshsub(ctl,vx0,vy0,
                      vx0+sht->bxsize,vy0+sht->bysize);
  }
  return ;
}

//隐藏图层 == 降低高度到-1
void sheet_free(struct SHEETCTL *ctl,struct SHEET *sht)
{
  if (sht->height >= 0)
  {
    sheet_updown(ctl,sht,-1);
  }
  sht->flags = 0;
  return ;
}
