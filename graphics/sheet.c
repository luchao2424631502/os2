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
  ctl = (struct SHEETCTL *)sys_malloc(sizeof(struct SHEETCTL));
  if (!ctl)
  {
    panic("graphics/sheet.c struct SHEETCTL initialize fail");
  }
  //给新的map结构分配和屏幕一样大小的内存 320*200Bytes
  ctl->map = (unsigned char *)sys_malloc(MAP_SIZE);
  if (!ctl->map)
  {
    panic("graphics/sheet.c ctl->map initialize fail");
  }

  ctl->vram = vram;
  ctl->xsize = xsize;
  ctl->ysize = ysize;
  //看起来图层最底高度是-1
  ctl->top = -1;
  for (int i=0; i<MAX_SHEETS; i++)
  {
    ctl->sheets0[i].flags = 0;
    ctl->sheets0[i].ctl = ctl;
  }
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
void sheet_updown(struct SHEET *sht,int height)
{
  struct SHEETCTL *ctl = sht->ctl;
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
      //此时h是等于height的,(重新刷新height以上的)
      ctl->sheets[height] = sht;
      sheet_refreshmap(ctl,sht->vx0,sht->vy0,
                        sht->vx0+sht->bxsize,sht->vy0+sht->bysize,height+1);
      sheet_refreshsub(ctl,sht->vx0,sht->vy0,
                        sht->vx0+sht->bxsize,sht->vy0+sht->bysize,height+1,old);
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
      sheet_refreshmap(ctl,sht->vx0,sht->vy0,
                      sht->vx0+sht->bxsize,sht->vy0+sht->bysize,0);
      //变化完高度(区域是不变的),刷新指定区域的所有图层,(所有高度的图层的重新刷新)
      sheet_refreshsub(ctl,sht->vx0,sht->vy0,
                      sht->vx0+sht->bxsize,sht->vy0+sht->bysize,0,old-1);
    }
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
    sheet_refreshmap(ctl,sht->vx0,sht->vy0,
                      sht->vx0+sht->bxsize,sht->vy0+sht->bysize,height);
    sheet_refreshsub(ctl,sht->vx0,sht->vy0,
                      sht->vx0+sht->bxsize,sht->vy0+sht->bysize,height,height);
  }
  return ;
}

/*
 * (bx0,by0)
 * (bx1,by1)
 * 给出的坐标是相对的,刷新对于这个图层的相对区域
 * 
 * */
void sheet_refresh(struct SHEET *sht,int bx0,int by0,int bx1,int by1)
{
  if (sht->height >= 0)
  {
    sheet_refreshsub(sht->ctl,sht->vx0+bx0,sht->vy0+by0,
                      sht->vx0+bx1,sht->vy0+by1,
                      sht->height,sht->height);
  }
  return ;
}

void sheet_refreshmap(struct SHEETCTL *ctl,
                      int vx0,int vy0,
                      int vx1,int vy1,
                      int h0)
{
  unsigned char *buf;
  unsigned char *map = ctl->map;
  struct SHEET *sht;

  //修正绝对地址
  if (vx0 < 0) { vx0 = 0; }
  if (vy0 < 0) { vy0 = 0; }
  if (vx1 > ctl->xsize) { vx1 = ctl->xsize; }
  if (vy1 > ctl->ysize) { vy1 = ctl->ysize; }

  //从高度h0向上开始
  for (int h=h0; h<=ctl->top; h++)
  {
    // 在sheets0[]下标作为图层的id
    sht = ctl->sheets[h];
    unsigned char sid = sht - ctl->sheets0;
    buf = sht->buf;

    //只遍历图层所在的区域
    int bx0 = vx0 - sht->vx0;
    int by0 = vy0 - sht->vy0;
    int bx1 = vx1 - sht->vx0;
    int by1 = vy1 - sht->vy0;
    if (bx0 < 0) { bx0 = 0; }
    if (by0 < 0) { by0 = 0; }
    if (bx1 > sht->bxsize) { bx1 = sht->bxsize; }
    if (by1 > sht->bysize) { by1 = sht->bysize; }

    for (int by=by0; by<by1; by++)
    {
      int vy = sht->vy0 + by;
      for (int bx=bx0; bx<bx1; bx++)
      {
        int vx = sht->vx0 + bx;
        //当前像素点不是透明色
        if (buf[by*sht->bxsize + bx] != sht->col_inv)
          //标记该点是图层sid的
          map[vy*ctl->xsize + vx] = sid;
      }
    }
  }
  return ;
}

/*
 * (vx0,vy0):屏幕上的左上坐标
 * (vx1,vy1):屏幕上的右下坐标
 * 给出的屏幕的两个坐标是绝对的,是要刷新这个绝对区域的所有图层
 * */
void sheet_refreshsub(struct SHEETCTL *ctl,int vx0,int vy0,int vx1,int vy1,int h0,int h1)
{
  unsigned char *buf;
  unsigned char *vram = ctl->vram;
  unsigned char *map = ctl->map;
  struct SHEET *sht;

  //控制刷新的绝对区域在(0,0)到(320,200)内 (VGA显存内)
  if (vx0 < 0) { vx0 = 0; }
  if (vy0 < 0) { vy0 = 0; }
  if (vx1 > ctl->xsize) { vx1 = ctl->xsize; }
  if (vy1 > ctl->ysize) { vy1 = ctl->ysize; }

  //对于每个图层来说 根据绝对区域裁剪出对于此图层的相对区域,然后拿到图层的像素,然后刷新
  for (int h=h0; h<=h1; h++)
  {
    sht = ctl->sheets[h];
    buf = sht->buf;
    unsigned char sid = sht - ctl->sheets0;

    //因为只有图层buf内的范围才有刷新的必要,所以找到绝对区域内的图层区域来做刷新
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
        //是该点像素所属的最高图层,则显示
        if (sid == map[vy*ctl->xsize + vx])
        {
          vram[vy*ctl->xsize + vx] = ch;
        }
      }
    }
  }
}


//移动图层(改变图层的原点的坐标就行)
void sheet_slide(struct SHEET *sht,int vx0,int vy0)
{
  int old_vx0 = sht->vx0;
  int old_vy0 = sht->vy0;
  sht->vx0 = vx0;
  sht->vy0 = vy0;
  //正在显示则移动图层后要重新绘制
  if (sht->height >= 0)
  {
    sheet_refreshmap(sht->ctl,old_vx0,old_vy0,
                      old_vx0+sht->bxsize,old_vy0+sht->bysize,0);
    sheet_refreshmap(sht->ctl,vx0,vy0,
                      vx0+sht->bxsize,vy0+sht->bysize,sht->height);
    //刷新移动原来的图层(目的是消除原来当前图层遗留下来的像素,所以从背景(height=0)开始)
    sheet_refreshsub(sht->ctl,old_vx0,old_vy0,
                      old_vx0+sht->bxsize,old_vy0+sht->bysize,0,sht->height-1);
    //刷新移动后的图层(只需要从当前图层高度开始刷新就行)
    sheet_refreshsub(sht->ctl,vx0,vy0,
                      vx0+sht->bxsize,vy0+sht->bysize,sht->height,sht->height);
  }
  return ;
}

//隐藏图层 == 降低高度到-1
void sheet_free(struct SHEET *sht)
{
  if (sht->height >= 0)
  {
    sheet_updown(sht,-1);
  }
  sht->flags = 0;
  return ;
}
