[bits 32]
%define ERROR_CODE nop
%define ZERO push 0

extern idt_table

section .data
global intr_entry_table
intr_entry_table:;引用所有中断程序的地址

%macro VECTOR 2
section .text
intr%1entry:
  %2        ;自动入栈的就添加nop就行
  push ds
  push es
  push fs
  push gs
  pushad

  ;向8259A发送EOI
  mov al,0x20 
  out 0xa0,al
  out 0x20,al

  push %1
  call [idt_table+%1*4]
  jmp intr_exit

section .data
  dd intr%1entry
%endmacro

section .text
global intr_exit
intr_exit:
  add esp,4   ;跳过自己push的中断号
  popad
  pop gs
  pop fs
  pop es
  pop ds
  add esp,4   ;跳过自动入栈的error_code

  iretd       ;中断返回

VECTOR 0x00,ZERO
VECTOR 0x01,ZERO
VECTOR 0x02,ZERO
VECTOR 0x03,ZERO 
VECTOR 0x04,ZERO
VECTOR 0x05,ZERO
VECTOR 0x06,ZERO
VECTOR 0x07,ZERO 
VECTOR 0x08,ERROR_CODE
VECTOR 0x09,ZERO
VECTOR 0x0a,ERROR_CODE
VECTOR 0x0b,ERROR_CODE 
VECTOR 0x0c,ZERO
VECTOR 0x0d,ERROR_CODE
VECTOR 0x0e,ERROR_CODE
VECTOR 0x0f,ZERO 
VECTOR 0x10,ZERO
VECTOR 0x11,ERROR_CODE
VECTOR 0x12,ZERO
VECTOR 0x13,ZERO 
VECTOR 0x14,ZERO
VECTOR 0x15,ZERO
VECTOR 0x16,ZERO
VECTOR 0x17,ZERO 
VECTOR 0x18,ERROR_CODE
VECTOR 0x19,ZERO
VECTOR 0x1a,ERROR_CODE
VECTOR 0x1b,ERROR_CODE 
VECTOR 0x1c,ZERO
VECTOR 0x1d,ERROR_CODE
VECTOR 0x1e,ERROR_CODE
VECTOR 0x1f,ZERO 

;8259A
VECTOR 0x20,ZERO  ;时钟中断
VECTOR 0x21,ZERO  ;irq1 键盘
VECTOR 0x22,ZERO  ;irq2 级联
VECTOR 0x23,ZERO  ;串口2
VECTOR 0x24,ZERO  ;串口1
VECTOR 0x25,ZERO  ;并口2
VECTOR 0x26,ZERO  ;软盘
VECTOR 0x27,ZERO  ;irq7 并口1

VECTOR 0x28,ZERO  ;级联8259a: irq8:实时时钟
VECTOR 0x29,ZERO  ;irq9: 重定向的irq2
VECTOR 0x2a,ZERO  ;irq10: 保留
VECTOR 0x2b,ZERO  ;irq11: 保留
VECTOR 0x2c,ZERO  ;irq12: ps/2鼠标
VECTOR 0x2d,ZERO  ;irq13: fpu异常
VECTOR 0x2e,ZERO  ;irq14: 硬盘
VECTOR 0x2f,ZERO  ;irq15: 保留

;---- 2021-4-28 添加0x80中断 ----
[bits 32]
extern syscall_table;系统调用函数表
section .text
global syscall_handler
syscall_handler:
  push 0 ;ZERO

  push ds
  push es
  push fs
  push gs

  pushad

  ;没有像上边一样现在发送EOI
  ;因为中断不是从8259A发生的,而是cpu内部指令产生的

  push 0x80

; 系统调用的参数传递顺序由ABI规定好了,然后此处从右向左入栈就行,(此os最多支持3个参数的系统调用
  push edx
  push ecx
  push ebx 

; 进入具体系统调用
  call [syscall_table + eax*4]
  add esp,12    ;c call 调用者来清理栈

;将栈中经过pushad后的eax的值强制修改为上面系统调用函数的返回值
  mov [esp + 8*4],eax 

;调用 通用的中断退出栈清理函数
  jmp intr_exit
