#ifndef __GRAPHICS_INCLUDE_FONT_H
#define __GRAPHICS_INCLUDE_FONT_H

#include "stdint.h"

extern char font_A[16];

/*给定ascii字符,得到字符的像素字节地址*/
char *font_vaddr(char);

/*给定字体的像素字节的地址,显示字体*/
void putfont8(uint8_t *,int ,int ,int ,uint8_t ,char *);
/*给定字符串,显示字符串字体*/
void putfont8_str(uint8_t *,int,int,int,uint8_t,char *);
/*给定uint32 整数,显示整数,暂时用来debug*/
void putfont8_int(uint8_t *,int,int,int,uint8_t,uint32_t);

#endif
