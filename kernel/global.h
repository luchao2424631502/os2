#ifndef __KERNEL_GLOBAL_H
#define __KERNEL_GLOBAL_H

#include "stdint.h"

/*GDT描述符属性*/
#define DESC_G_4K   1
#define DESC_D_32   1
#define DESC_L      0   //64bit代码段标记
#define DESC_AVL    0   //保留位
#define DESC_P      1
#define DESC_DPL_0  0
#define DESC_DPL_1  1
#define DESC_DPL_2  2
#define DESC_DPL_3  3

#define DESC_S_CODE 1
#define DESC_S_DATA DESC_S_CODE
#define DESC_S_SYS  0
#define DESC_TYPE_CODE    8
#define DESC_TYPE_DATA    2
#define DESC_TYPE_TSS     9


#define RPL0 0
#define RPL1 1 
#define RPL2 2
#define RPL3 3

#define TI_GDT 0
#define TI_LDT 1

#define SELECTOR_K_CODE ((1<<3) + (TI_GDT<<2) + RPL0)
#define SELECTOR_K_DATA ((2<<3) + (TI_GDT<<2) + RPL0)
#define SELECTOR_K_STACK SELECTOR_K_DATA 
#define SELECTOR_K_GS   ((3<<3) + (TI_GDT<<2) + RPL0)
/*此处是第4个描述符,代表tss,*/
#define SELECTOR_U_CODE  ((5<<3) + (TI_GDT<<2) + RPL3)
#define SELECTOR_U_DATA  ((6<<3) + (TI_GDT<<2) + RPL3)
#define SELECTOR_U_STACK SELECTOR_U_DATA

/*GDT描述符高位属性*/
#define GDT_ATTR_HIGH ((DESC_G_4K << 7) + (DESC_D_32 << 6) + (DESC_L << 5) + (DESC_AVL << 4))
#define GDT_CODE_ATTR_LOW_DPL3 ((DESC_P << 7) + (DESC_DPL_3 << 5) + (DESC_S_CODE << 4) + DESC_TYPE_CODE)
#define GDT_DATA_ATTR_LOW_DPL3 ((DESC_P << 7) + (DESC_DPL_3 << 5) + (DESC_S_DATA << 4) + DESC_TYPE_DATA)

/*TSS属性描述*/
#define TSS_DESC_D  0

#define TSS_ATTR_HIGH ((DESC_G_4K << 7) + (TSS_DESC_D << 6) + (DESC_L << 5) + (DESC_AVL << 4) + 0x0)
#define TSS_ATTR_LOW  ((DESC_P << 7) + (DESC_DPL_0 << 5) + (DESC_S_SYS << 4) + DESC_TYPE_TSS)
#define SELECTOR_TSS    ((4<<3) + (TI_GDT<<2) + RPL0)

/*IDT描述符属性*/
#define IDT_DESC_P 1
#define IDT_DESC_DPL0 0
#define IDT_DESC_DPL3 3
#define IDT_DESC_32_TYPE 0xE
#define IDT_DESC_16_TYPE 0x6
#define IDT_DESC_ATTR_DPL0 ((IDT_DESC_P<<7)+(IDT_DESC_DPL0<<5)+IDT_DESC_32_TYPE)
#define IDT_DESC_ATTR_DPL3 ((IDT_DESC_P<<7)+(IDT_DESC_DPL3<<5)+IDT_DESC_32_TYPE)

struct gdt_desc
{
  uint16_t limit_low_word; //段界限 15-0
  uint16_t base_low_word;  //段基址 15-0
  uint8_t  base_mid_byte;  //段基址 23-16
  uint8_t  attr_low_byte;  //属性的低8 bit
  uint8_t  limit_high_attr_high; //段界限19-16 + 属性的高4bit
  uint8_t  base_high_byte; //段基址 31-24
};

/********************************************************
--------------------------------------------------------------
		  Intel 8086 Eflags Register
--------------------------------------------------------------
*
*     15|14|13|12|11|10|F|E|D C|B|A|9|8|7|6|5|4|3|2|1|0|
*      |  |  |  |  |  | | |  |  | | | | | | | | | | | '---  CF……Carry Flag
*      |  |  |  |  |  | | |  |  | | | | | | | | | | '---  1 MBS
*      |  |  |  |  |  | | |  |  | | | | | | | | | '---  PF……Parity Flag
*      |  |  |  |  |  | | |  |  | | | | | | | | '---  0
*      |  |  |  |  |  | | |  |  | | | | | | | '---  AF……Auxiliary Flag
*      |  |  |  |  |  | | |  |  | | | | | | '---  0
*      |  |  |  |  |  | | |  |  | | | | | '---  ZF……Zero Flag
*      |  |  |  |  |  | | |  |  | | | | '---  SF……Sign Flag
*      |  |  |  |  |  | | |  |  | | | '---  TF……Trap Flag
*      |  |  |  |  |  | | |  |  | | '---  IF……Interrupt Flag
*      |  |  |  |  |  | | |  |  | '---  DF……Direction Flag
*      |  |  |  |  |  | | |  |  '---  OF……Overflow flag
*      |  |  |  |  |  | | |  '----  IOPL……I/O Privilege Level
*      |  |  |  |  |  | | '-----  NT……Nested Task Flag
*      |  |  |  |  |  | '-----  0
*      |  |  |  |  |  '-----  RF……Resume Flag
*      |  |  |  |  '------  VM……Virtual Mode Flag
*      |  |  |  '-----  AC……Alignment Check
*      |  |  '-----  VIF……Virtual Interrupt Flag
*      |  '-----  VIP……Virtual Interrupt Pending
*      '-----  ID……ID Flag
*
*
**********************************************************/
#define EFLAGS_MBS    (1 << 1)
#define EFLAGS_IF_1   (1 << 9)
#define EFLAGS_IF_0   0
#define EFLAGS_IOPL_3 (3 << 12) //允许ring3用户进程直接进行IO
#define EFLAGS_IOPL_0 (0 << 12) //IOPL0


#define NULL ((void*)0)
#define DIV_ROUND_UP(X,STEP) ((X+STEP - 1) / (STEP)) //向上取整
#define bool int
#define true  1
#define false 0

#define PG_SIZE 4096

#define UNUSED  __attribute__ ((unused))

#endif
