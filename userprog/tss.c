#include "tss.h"
#include "stdint.h"
#include "global.h"
#include "string.h"
#include "print.h"

/*tss结构*/
struct tss
{
  uint32_t backlink;
  uint32_t *esp0;
  uint32_t ss0;
  uint32_t *esp1;
  uint32_t ss1;
  uint32_t *esp2;
  uint32_t ss2;
  uint32_t cr3;
  uint32_t (*eip)(void); /*eip函数指针?*/
  uint32_t eflags;
  uint32_t eax;
  uint32_t ecx;
  uint32_t edx;
  uint32_t ebx;
  uint32_t esp;
  uint32_t ebp;
  uint32_t esi;
  uint32_t edi;
  uint32_t es;
  uint32_t cs;
  uint32_t ss;
  uint32_t ds;
  uint32_t fs;
  uint32_t gs;
  uint32_t ldt;   //ldt选择子
  uint32_t trace; //io bit map在tss中的偏移地址
  uint32_t io_base;
};

static struct tss tss;

/*更新tss中的esp0,*/
void update_tss_esp(struct task_struct *thread)
{
  //pcb的高地址是内核栈顶
  tss.esp0 = (uint32_t *)((uint32_t)thread+PG_SIZE);
}

/*创建gdt描述符*/
static struct gdt_desc make_gdt_desc(uint32_t *desc_addr,uint32_t limit,uint8_t attr_low,uint8_t attr_high)
{
  uint32_t desc_base = (uint32_t)desc_addr;
  struct gdt_desc desc;
  desc.limit_low_word = (limit & 0x0000ffff);
  desc.base_low_word = (desc_base & 0x0000ffff);
  desc.base_mid_byte = ((desc_base & 0x00ff0000) >> 16);
  desc.attr_low_byte = (uint8_t)(attr_low);
  desc.limit_high_attr_high = (((limit & 0x000f0000) >> 16)+ (uint8_t)(attr_high));
  // desc.base_high_byte = (desc_base & 0xff000000) >> 24;
  desc.base_high_byte = desc_base >> 24;
  return desc;
}

/*
 * 1.在gdt安装tss描述符
 * 2.在gdt安装2个供ring 3进程的描述符(code + data)
 * */
void tss_init()
{
  put_str("[tss_init] start\n");
  uint32_t tss_size = sizeof(tss);
  memset(&tss,0,tss_size);
  /*tss中ring 0内核栈赋值*/
  tss.ss0 = SELECTOR_K_STACK;
  tss.io_base = tss_size;

  /*gdt放在0x900处,则从0x900+4*0x8=0x900+0x20处开始添加*/
  /* 1.添加tss描述符 */
  *((struct gdt_desc *)0xc0000920) = make_gdt_desc((uint32_t *)&tss,tss_size - 1,TSS_ATTR_LOW,TSS_ATTR_HIGH);

  /* 2.添加dpl=3的ring 3代码段和数据段描述符*/
  *((struct gdt_desc *)0xc0000928) = make_gdt_desc((uint32_t *)0,0xfffff,GDT_CODE_ATTR_LOW_DPL3,GDT_ATTR_HIGH);
  *((struct gdt_desc *)0xc0000930) = make_gdt_desc((uint32_t *)0,0xfffff,GDT_DATA_ATTR_LOW_DPL3,GDT_ATTR_HIGH);

  /*gdt 有6+1个描述符 limit = 7*8-1 */
  uint64_t gdt_oprand = ((7*8-1) | ((uint64_t)(uint32_t)0xc0000900 << 16));

  asm volatile ("lgdt %0"::"m"(gdt_oprand):); //重新加载gdt
  asm volatile ("ltr %w0"::"r"(SELECTOR_TSS):); //将tss选择子加载到(tss)register中
  
  put_str("[tss_init] done\n");
}
