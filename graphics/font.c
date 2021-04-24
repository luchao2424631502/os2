#include "graphics.h"
#include "font.h"

//测试字体A 8*16个像素
char font_A[16] = {
  0x00,0x18,0x18,0x18,0x18,0x24,0x24,0x24,
  0x24,0x7e,0x42,0x42,0x42,0xe7,0x00,0x00
};

//得到字体的char*地址,方便putfont8 的char* font参数
char* font_vaddr(char ch)
{
  return (char *)(FONT_ASCII_START + ch * FONT_SIZE);
}

//显示字符 8*16格式
void putfont8(uint8_t *vram,int xsize,int x,int y,uint8_t color,char *font)
{
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
        p[7-j] = color;
    }
  }
}

void make_font()
{

}
