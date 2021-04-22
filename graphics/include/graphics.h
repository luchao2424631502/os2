#ifndef __GRAPHICS_GRAPHICS_H
#define __GRAPHICS_GRAPHICS_H

/*调色板16个基础颜色*/
#include "color256.h"

/*VGA 320*200 模式下vram的物理起始地址*/
#define VGA_START_M 0xa0000
/*                  vram的虚拟起始地址*/
#define VGA_START_V 0xc00a0000

#define Width_320   320
#define Length_200  200
//
// 4/22添加图形debug函数
void graphics_init();
#endif
