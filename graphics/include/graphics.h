#ifndef __GRAPHICS_INCLUDE_GRAPHICS_H
#define __GRAPHICS_INCLUDE_GRAPHICS_H

#include "stdint.h"
/*调色板16个基础颜色*/
#include "color256.h"

/*!!! 2021/4/24: 已经将256个字符嵌入到kernel中了.地址是*/
#define FONT_ASCII_START 0xc00091c0
#define FONT_ASCII_END   0xc000a1c0
#define FONT_ASCII_SIZE  0x1000
#define FONT_SIZE        0x10


/*VGA 320*200 模式下vram的物理起始地址*/
#define VGA_START_M 0xa0000
/*                  vram的虚拟起始地址*/
#define VGA_START_V 0xc00a0000

#define Width_320   320
#define Length_200  200

//mbr中的graphics的引导信息
struct BOOT_INFO 
{
  uint8_t cyls,leds,vmode,reserve;
  uint16_t scrnx,scrny;
  uint8_t *vram;
};

// 4/22添加图形debug函数
void graphics_init();
#endif
