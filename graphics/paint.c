#include "paint.h"

/*填充一个矩形区域
 * vram:vram 虚拟地址
 * xwidth: 屏幕像素宽度
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
