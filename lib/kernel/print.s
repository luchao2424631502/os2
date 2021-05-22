TI_GDT  equ 0
RPL0  equ 0
SELECTOR_VIDEO  equ (0x3<<3) + TI_GDT + RPL0

section .data
put_int_buffer dq 0

[bits 32]
section .text
global put_str
global init_reg
;清零光标寄存器的初始值
init_reg:
  pushad
  mov dx,0x3d4
  mov al,0x0e
  out dx,al
  mov dx,0x3d5
  xor eax,eax
  out dx,al

  mov dx,0x3d4
  mov al,0x0f
  out dx,al
  mov dx,0x3d5
  xor eax,eax
  out dx,al

  popad
  ret

global set_cursor
;接收光标位置参数
set_cursor:
  pushad 
  mov bx,[esp+36]

  mov dx,0x3d4
  mov al,0x0e
  out dx,al
  mov dx,0x3d5
  mov al,bh
  out dx,al

  mov dx,0x3d4
  mov al,0x0f
  out dx,al
  mov dx,0x3d5
  mov al,bl
  out dx,al

  popad
  ret

put_str:
  push ebx
  push ecx
  xor ecx,ecx
  mov ebx,[esp + 12]
.goon:
  mov cl,[ebx]
  cmp cl,0
  jz .str_over
  push ecx
  call put_char
  add esp,4   ;调用者来清理栈
  inc ebx
  jmp .goon
.str_over:
  pop ecx
  pop ebx
  ret

global put_char
put_char:
  pushad
  mov ax,SELECTOR_VIDEO
  mov gs,ax

  ;获得当前光标位置
  mov dx,0x3d4
  mov al,0x0e
  out dx,al
  mov dx,0x3d5
  in al,dx
  mov ah,al

  mov dx,0x3d4
  mov al,0x0f
  out dx,al
  mov dx,0x3d5
  in al,dx

  ;bx=光标位置,ecx=字符参数
  mov bx,ax
  mov ecx,[esp+36]

  cmp cl,0xd
  jz .is_carring_return   ;回车
  cmp cl,0xa
  jz .is_line_feed        ;换行
  cmp cl,0x8
  jz .is_backspace        ;退格
  
  jmp .put_other

.is_backspace:
  dec bx
  shl bx,1    ;bx*2
  mov byte [gs:bx],0x20   ;空格的ascii
  inc bx
  mov byte [gs:bx],0x07   ;颜色
  shr bx,1    ;还原成光标位置
  jmp .set_cursor

.put_other:
  shl bx,1

  mov [gs:bx],cl
  inc bx
  mov byte [gs:bx],0x07
  shr bx,1
  inc bx 
  cmp bx,2000
  jl .set_cursor

  ;\r \n都像linux一样处理成\n(换行+回车)
.is_line_feed:          ;\n
.is_carring_return:     ;\r:光标回到行首
  xor dx,dx
  mov ax,bx             ;bx现在是光标的位置 
  mov si,80
  div si                ;ax=当前光标在第几行,dx=第几列
  sub bx,dx             ;bx-dx=行首

.is_carring_return_end:
  add bx,80
  cmp bx,2000
.is_line_feed_end:
  jl .set_cursor

  ;开始滚屏,1-24行搬到0-23行,24行填0
.roll_screen:
  cld
  mov ecx,960
  mov esi,0xc00b80a0
  mov edi,0xc00b8000
  rep movsd

  mov ebx,3840          ;24行的起始偏移字节
  mov ecx,80
  .cls:
  mov word [gs:ebx],0x0720
  add ebx,2
  loop .cls
  mov bx,1920           ;光标重新设置到最后一行开头

.set_cursor:;将bx中的光标值设置到显卡寄存器中
  ;设置高8bit
  mov dx,0x3d4
  mov al,0x0e
  out dx,al
  mov dx,0x3d5
  mov al,bh
  out dx,al

  ;设置低8bit
  mov dx,0x3d4
  mov al,0x0f
  out dx,al
  mov dx,0x3d5
  mov al,bl
  out dx,al

.put_char_done:
  popad
  ret

global cls_screen
cls_screen:
  pushad 

  mov ax,SELECTOR_VIDEO
  mov gs,ax

  mov ebx,0
  mov ecx,80*25
.cls:
  mov word [gs:ebx],0x0720;黑底白字空白键
  add ebx,2
  loop .cls
  mov ebx,0

.set_cursor:;将bx中的光标值设置到显卡寄存器中
  ;设置高8bit
  mov dx,0x3d4
  mov al,0x0e
  out dx,al
  mov dx,0x3d5
  mov al,bh
  out dx,al

  ;设置低8bit
  mov dx,0x3d4
  mov al,0x0f
  out dx,al
  mov dx,0x3d5
  mov al,bl
  out dx,al

  popad
  ret



global put_int

;打印16进制数字
put_int:
  pushad
  mov ebp,esp
  mov eax,[ebp+4*9]     ;需要打印的参数int值
  mov edx,eax
  mov edi,7             
  mov ecx,8
  mov ebx,put_int_buffer
  ;将数字转换后的字符填充到put_int_buffer
.16based_4bits:
  and edx,0x0000000F
  cmp edx,9
  jg .is_A2F            ;jg 大于就跳转
  add edx,'0'
  jmp .store
.is_A2F:
  sub edx,10
  add edx,'A'
.store:
  mov [ebx + edi],dl
  dec edi               ;从后往前
  shr eax,4
  mov edx,eax
  loop .16based_4bits

  ;在打印前去掉高位多余的0
.ready_to_print:
  inc edi               ;edi=0
.skip_prefix_0:
  cmp edi,8
  je .full0  
.go_on_skip:
  mov cl,[put_int_buffer+edi]
  inc edi
  cmp cl,'0'
  je .skip_prefix_0
  dec edi
  jmp .put_each_num
.full0:
  mov cl,'0'
  ;打印每一个非0字符
.put_each_num:
  push ecx
  call put_char
  add esp,4
  inc edi
  mov cl,[put_int_buffer+edi]
  cmp edi,8
  jl .put_each_num      ;小于则转移

  popad 
  ret

