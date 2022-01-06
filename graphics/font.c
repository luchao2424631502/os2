#include "graphics.h"
#include "font.h"
#include "string.h"
#include <stdio.h>

extern uint8_t _binary_graphics_1_in_start[];
extern uint8_t _binary_graphics_1_in_end[];
extern uint8_t _binary_graphics_1_in_size[];

//测试字体A 8*16个像素
char font_A[16] = {
  0x00,0x18,0x18,0x18,0x18,0x24,0x24,0x24,
  0x24,0x7e,0x42,0x42,0x42,0xe7,0x00,0x00
};

/*
 * 得到字体的char*地址,方便putfont8 的char* font参数
 * !!!注意!!!字体最后写入到kernel.bin的地址是不固定的,bug找了半天才发现
*/
static char* font_vaddr(char ch)
{
  uint32_t vaddr = (uint32_t)_binary_graphics_1_in_start;
  return (char *)(vaddr + ch * FONT_SIZE);
}

//显示字符 8*16格式
void putfont8(uint8_t *vram,int xsize,int x,int y,
    uint8_t color,char ch)
{
  char *font = font_vaddr(ch);
  uint8_t data;
  uint8_t *p;
  int and[8] = {0x1,0x2,0x4,0x8,0x10,0x20,0x40,0x80};
  for (int i=0; i<16; i++)
  {
    p = vram + ((y + i) * xsize) + x;
    data = font[i]; //字体
    for (int j=0; j<8; j++)
    {
      if (data & and[j])
        p[7-j] = color;//具体像显存地址处写入的是颜色index
    }
  }
}

//给定字符串,显示出来
void putfont8_str(uint8_t *vram,int xsize,int x,int y,
    uint8_t color,char *str)
{
  for (; *str != 0; str++)
  {
    // putfont8(vram,xsize,x,y,color,font_vaddr(*str));
    putfont8(vram,xsize,x,y,color,*str);
    x += 8;
  }
}

//显示10进制整数
void putfont8_int(uint8_t *vram,int xsize,int x,int y,uint8_t color,uint32_t num)
{
  char ans[15];
  memset(ans,0,10);

  sprintf(ans,"%d",num);
  putfont8_str(vram,xsize,x,y,color,ans);
}

void putfont8_hex(uint8_t *vram,int xsize,int x,int y,uint8_t color,uint32_t num)
{
  char ans[16];
  memset(ans,0,16);

  sprintf(ans,"%x",num);
  putfont8_str(vram,xsize,x,y,color,ans);
}
