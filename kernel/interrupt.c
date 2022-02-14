#include "interrupt.h"
#include "stdint.h"
#include "global.h"
#include "io.h"
#include "print.h"

#define PIC_M_CTRL 0x20 //主片控制端口
#define PIC_M_DATA 0x21
#define PIC_S_CTRL 0xa0 //从片
#define PIC_S_DATA 0xa1

/*2021-4-28:为了支持0x80中断,
 * 所以增加中断数组大小到0x81
 * syscall_handler表示0x80中断的处理函数 
 * */
#define IDT_DESC_CNT  0x81  

#define EFLAGS_IF 0x00000200
#define GET_EFLAGS(EFLAG_VAR) asm volatile ("pushfl; popl %0":"=g"(EFLAG_VAR))
extern uint32_t syscall_handler(void);

struct gate_desc{
  uint16_t func_offset_low_word;
  uint16_t selector;
  uint8_t dcount;
  uint8_t attribute;
  uint16_t func_offset_high_word;
};

static void make_idt_desc(struct gate_desc*,uint8_t,intr_handler);

static struct gate_desc idt[IDT_DESC_CNT]; //IDT表自定义中断的描述符
char *intr_name[IDT_DESC_CNT];
intr_handler idt_table[IDT_DESC_CNT]; //C中的中断处理程序入口
extern intr_handler intr_entry_table[IDT_DESC_CNT]; //引用kernel.s中定义的中断处理程序入口 

/*初始化可编程中断控制器8259A*/
static void pic_init()
{
  //初始化主片
  outb(PIC_M_CTRL,0x11); //ICW1: 边沿触发,级联从片
  outb(PIC_M_DATA,0x20); //ICW2: 起始向量号0x20
  outb(PIC_M_DATA,0x04); //ICW3: 主片IR2接从片
  outb(PIC_M_DATA,0x01); //ICW4: 正常EOI

  //初始化从片
  outb(PIC_S_CTRL,0x11); //ICW1
  outb(PIC_S_DATA,0x28); //ICW2: 从片的起始中断向量号从0x28开始
  outb(PIC_S_DATA,0x02); //ICW3: 从片连接到主片的IR2
  outb(PIC_S_DATA,0x01); //ICW4: 正常EOI

	//暂时先关闭所有的中断,等到timer_init在开启需要的中断
	outb(PIC_M_DATA,0xff);
	outb(PIC_S_DATA,0xff);
	
  //打开主片IR0的时钟中断
  // outb(PIC_M_DATA,0xfe);
  // outb(PIC_S_DATA,0xff);
  // <<<<<<< HEAD

  //同时打开键盘中断和时钟中断,+ps/2鼠标中断
  // outb(PIC_M_DATA,0xfc);
  // outb(PIC_S_DATA,0xef);

  //测试:打开键盘中断+irq2+鼠标中断
  // outb(PIC_M_DATA,0xf9);
  // outb(PIC_S_DATA,0xef);
  // =======

  //同时打开键盘中断和时钟中断
  // outb(PIC_M_DATA,0xfc);
  // outb(PIC_S_DATA,0xff);

  //键盘+时钟+irq2+硬盘
  // outb(PIC_M_DATA,0xf8);
  // outb(PIC_S_DATA,0xbf);
  // >>>>>>> master

	//合并后应该开启的中断:时钟+irq2+键盘+鼠标+硬盘
	//outb(PIC_M_DATA,0xf8);
	//outb(PIC_S_DATA,0xaf);

	/*
	 *2022-02-14: 因为要设置intel8253定时器芯片,所以暂时先关闭中断,等设置完后在开启时钟中断
	 *outb(PIC_M_DATA,0xf9);
	 *outb(PIC_S_DATA,0xaf);
	 */

  put_str("    pic_init done\n");
}

/*构造中断门描述符*/
static void make_idt_desc(struct gate_desc* p_gdesc,
    uint8_t attr,intr_handler function)
{
  p_gdesc->func_offset_low_word = (uint32_t)function & 0x0000ffff;
  p_gdesc->selector = SELECTOR_K_CODE;
  p_gdesc->dcount = 0;
  p_gdesc->attribute = attr;
  p_gdesc->func_offset_high_word = ((uint32_t)function & 0xffff0000)>>16;
}

/*初始化中断描述符表,*/
static void idt_desc_init()
{
  for (int i=0; i<IDT_DESC_CNT; i++)
  {
    make_idt_desc(&idt[i],IDT_DESC_ATTR_DPL0,intr_entry_table[i]);
  }
  /*2021-4-28:0x80中断实现系统调用,
   * 描述符单独初始化,dpl=3,ring 3用户进程就能产生并使用0x80中断,
   * 并且中断处理程序也不在intr_entry_table[]中,意思是栈操作自己重写
   * */
  make_idt_desc(&idt[0x80],IDT_DESC_ATTR_DPL3,syscall_handler);
  put_str("    idt_desc_init done\n");
}

/*通用中断处理函数*/
static void general_intr_handler(uint8_t vec_nr)
{
  if (vec_nr == 0x27 || vec_nr == 0x2f)
  {
    return ;//irq7 和 irq15产生伪中断,屏蔽
  }

  set_cursor(0);
  int cursor_pos = 0;
  while(cursor_pos < 320)
  {
    put_char(' ');
    cursor_pos++;
  }

  set_cursor(0);
  put_str("[---- excetion ----]\n");
  put_str("    int vector : 0x");
  put_int(vec_nr);
  
  put_str("\n    ");
  put_str(intr_name[vec_nr]);
  put_str("\n");
  if (vec_nr == 14)
  {
    int page_fault_vaddr = 0;
    asm("movl %%cr2,%0":"=r"(page_fault_vaddr)::);
    put_str("page_fault_vaddr is ");
    put_int(page_fault_vaddr);
  }
  
  put_str("\n[---- excetion ----]\n");
  while(1){}
}

/*一般中断处理函数注册 和 异常名称注册*/
static void exception_init()
{
  //统一赋默认值
  for (int i=0; i<IDT_DESC_CNT; i++)
  {
    idt_table[i] = general_intr_handler;
    intr_name[i] = "unknown";
  }
  
  intr_name[0] = "#DE Divide Error";
  intr_name[1] = "#DB Debug Exception";
  intr_name[2] = "NMI Interrupt";
  intr_name[3] = "#BP BreakPoint Exception";
  intr_name[4] = "#OF Overflow Exception";
  intr_name[5] = "#BR Bound Range Exceeded Exception";
  intr_name[6] = "#UD Invalid Opcode Exception";
  intr_name[7] = "#NM Device Not Available Exception";
  intr_name[8] = "#DF Double Fault Exception";
  intr_name[9] = "Coprocesser Segment Overrun";
  intr_name[10]= "#TS Invalid TSS Exception";
  intr_name[11]= "#NP Segment Not Present";
  intr_name[12]= "#SS Stack Fault Exception";
  intr_name[13]= "#GP General Protection Exception";
  intr_name[14]= "#PF Page-Fault Exception";
  //intr_name[15]保留
  intr_name[16]= "#MF x87 FPU Floating-Point Error";
  intr_name[17]= "#AC Alignment Check Exception";
  intr_name[18]= "#MC Machine-Check Exception";
  intr_name[19]= "#XF SIMD Floating-Point Exception";

  intr_name[0x21] = "#KeyBoard";
}


/*开中断*/
enum intr_status intr_enable()
{
  enum intr_status old_status;
  if (INTR_ON == intr_get_status())
  {
    old_status = INTR_ON;
    return old_status;
  }
  else 
  {
    old_status = INTR_OFF;
    asm volatile ("sti");//开中断
    return old_status;
  }
}

/*关中断*/
enum intr_status intr_disable()
{
  enum intr_status old_status;
  if (INTR_ON == intr_get_status())
  {
    old_status = INTR_ON;
    asm volatile ("cli":::"memory");//关中断
    return old_status;
  }
  else 
  {
    old_status = INTR_OFF;
    return old_status;
  }
}

/*将中断状态设置status*/
enum intr_status intr_set_status(enum intr_status status)
{
  return status & INTR_ON ? intr_enable() : intr_disable();
}

/*得到当前中断状态*/
enum intr_status intr_get_status()
{
  uint32_t eflags = 0;
  GET_EFLAGS(eflags);//得到当前的EFLAGS寄存器
  return (EFLAGS_IF & eflags) ? INTR_ON : INTR_OFF;
}


/*注册中断号对应的中断处理函数*/
void register_handler(uint8_t vector_no,intr_handler function)
{
  //kernel.s中call idt_table[]函数
  idt_table[vector_no] = function;
}

/*中断初始化*/
void idt_init()
{
  put_str("[idt_init] start\n");
  
  idt_desc_init(); //构造idt中的描述符
  exception_init(); //异常名初始化并注册通常中断处理函数
  pic_init();//初始化8259A

  //将构造的idt表地址加载到IDTR寄存器中
  uint64_t idt_operand = ((sizeof(idt)-1) | ((uint64_t)(uint32_t)idt<<16));
  asm volatile ("lidt %0"::"m"(idt_operand));

  put_str("[idt_init] done\n");
}
