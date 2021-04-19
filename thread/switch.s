[bits 32]
section .text
global switch_to

switch_to:
  push esi
  push edi
  push ebx
  push ebp

  ;20:cur_task, 24:next_task
  mov eax,[esp + 20]
  mov [eax],esp ;将内核中断代码的栈指针esp保存到要换下的task的pcb的栈指针中

  mov eax,[esp + 24]
  mov esp,[eax]

  pop ebp
  pop ebx
  pop edi
  pop esi
  ret ;返回
