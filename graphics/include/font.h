#ifndef __GRAPHICS_INCLUDE_FONT_H
#define __GRAPHICS_INCLUDE_FONT_H

#include "stdint.h"

extern char font_A[16];

void make_font();
void putfont8(uint8_t *,int ,int ,int ,uint8_t ,char *);
/*给定ascii字符,得到字符的像素字节地址*/
char *font_vaddr(char);

#endif
