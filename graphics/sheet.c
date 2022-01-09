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
    //移动完后,刷新整个屏幕
    sheet_refresh(ctl);
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
    sheet_refresh(ctl);
  }
  return ;
}

/*
 * (bx,by):相对于buf的坐标
 * (vx,vy):相对于显存vram的坐标
 * */
void sheet_refresh(struct SHEETCTL *ctl)
{
  unsigned char *buf,*vram = ctl->vram;
  struct SHEET *sht;
  //从下图层到上图层
  for (int h = 0; h<=ctl->top; h++)
  {
    sht = ctl->sheets[h];
    buf = sht->buf;
    int vx,vy;
    //从sheet上到下
    for (int by = 0; by<sht->bysize; by++)
    {
      //得到在vram中当前的y坐标
      vy = sht->vy0 + by;
      //固定了y,然后扫描y的这一行
      for (int bx = 0; bx<sht->bxsize; bx++)
      {
        //得到在vram中当前的x坐标
        vx = sht->vx0 + bx;
        //首先通过相对坐标bx,by拿到坐标对应的像素
        unsigned char c = buf[by * sht->bxsize + bx];
        //除col_inv颜色外的其它像素都显示
        if (c != sht->col_inv)
          vram[vy * ctl->xsize + vx] = c;
      }
    }
  }
  return ;
}

//移动图层(改变图层的原点的坐标就行)
void sheet_slide(struct SHEETCTL *ctl,struct SHEET *sht,int vx0,int vy0)
{
  sht->vx0 = vx0;
  sht->vy0 = vy0;
  //正在显示则移动图层后要重新绘制
  if (sht->height >= 0)
  {
    sheet_refresh(ctl);
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
