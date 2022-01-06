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

//设定图层的高度
void sheet_updown()
{

}
