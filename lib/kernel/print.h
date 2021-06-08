#ifndef __LIB_KERNEL_PRINT_H
#define __LIB_KERNEL_PRINT_H

#include "stdint.h"

void put_char(uint8_t);
void put_str(char*);
void init_reg();
void put_int(uint32_t);    //以16进制打印  
void set_cursor(uint32_t);
void cls_screen();

#endif
