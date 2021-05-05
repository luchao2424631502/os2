;简单的loader
%include "boot.inc"
section loader vstart=LOADER_BASE_ADDR

;GDT表的定义
GDT_BASE:
	dd 0x00000000
	dd 0x00000000
CODE_DESC:	
	dd 0x0000FFFF
	dd DESC_CODE_HIGH4
DATA_STACK_DESC:
	dd 0x0000FFFF
	dd DESC_DATA_HIGH4
VIDEO_DESC:	
	dd 0x80000007
	dd DESC_VIDEO_HIGH4

	GDT_SIZE	equ $-GDT_BASE
	GDT_LIMIT	equ GDT_SIZE-1
	times 60 dq 0
	SELECTOR_CODE equ (0x1<<3) + TI_GDT + RPL0
	SELECTOR_DATA equ (0x2<<3) + TI_GDT + RPL0
	SELECTOR_VIDEO equ (0x3<<3) + TI_GDT + RPL0

;-----0x900+0x200 = 0xb00
	total_mem_bytes dd 0
	gdt_ptr dw GDT_LIMIT
			dd GDT_BASE
	ards_buf times 244 db 0
	ards_nr dw 0
;------

;0x900+64*8+256 = 0x900+0x300= 0xb00
loader_start:;现在仍在实模式下
	xor ebx,ebx
	mov edx,0x534d4150

	mov di,ards_buf
;-----int 0x15h ax=0xe8200 -----
.e820_mem_get_loop:
	mov eax,0x0000e820
	mov ecx,20;固定ards结构大小20byte
	int 0x15
	jc .e820_failed_so_try_e801;cf=1表示调用出错
	add di,cx 
	inc word [ards_nr]			;记录adrs可用内存段的个数
	cmp ebx,0					;判断是否是最后一个adrs结构
	jnz .e820_mem_get_loop

	mov cx,[ards_nr]
	mov ebx,ards_buf
	xor edx,edx
.find_max_mem_area:
	mov eax,[ebx]
	add eax,[ebx+8]
	add ebx,20
	cmp edx,eax		;edx-eax
	jge .next_ards
	mov edx,eax		;edx<eax则 edx=eax
.next_ards:
	loop .find_max_mem_area
	jmp .mem_get_ok

;----- int 0x15h ax=e801h -----
.e820_failed_so_try_e801:
	mov ax,0xe801
	int 0x15
	jc .e801_failed_so_try88

	;计算低15mb的容量
	mov cx,0x400
	mul cx	;ax * cx = dx:ax
	shl edx,16
	and eax,0x0000FFFF
	or edx,eax 
	add edx,0x100000	;+1mb大小
	mov esi,edx
	;计算16mb以上大小
	xor eax,eax
	mov ax,bx 
	mov ecx,0x10000
	mul ecx			;eax*ecx = edx:eax
	add esi,eax
	mov edx,esi
	jmp .mem_get_ok
	
;------ int 0x15 ax=0x88 -----
.e801_failed_so_try88:
	mov ah,0x88
	int 0x15
	jc .error_hlt
	and eax,0x0000FFFF

	mov cx,0x400
	mul cx		;ax*cx = dx:ax
	shl edx,16
	or edx,eax
	add edx,0x100000

.mem_get_ok:
	mov [total_mem_bytes],edx

;----- 准备进入保护模式 -----

	in al,0x92
	or al,0000_0010B
	out 0x92,al

	lgdt [gdt_ptr]

	mov eax,cr0
	or eax,0x00000001
	mov cr0,eax

	;16bit编译的code,运行在32bit的保护模式下,所以加上dword
	jmp dword SELECTOR_CODE:p_mode_start

.error_hlt:
	hlt
;刷新流水线进入保护模式
[bits 32]
p_mode_start:
	mov ax,SELECTOR_DATA
	mov ds,ax
	mov es,ax 
	mov ss,ax
	mov esp,LOADER_STACK_TOP
	mov ax,SELECTOR_VIDEO
	mov gs,ax

;2021-3-8: 在分页开启前把elf格式的内核加载到0x70000
	mov eax,KERNEL_START_SECTOR
	mov ebx,KERNEL_BIN_BASE_ADDR ;1mb下的高地址:0x70000
	mov ecx,200
	call rd_disk_m_32

	call setup_page

	sgdt [gdt_ptr]

	;修改显存描述符的虚拟地址
	mov ebx,[gdt_ptr + 2]
	or dword [ebx + 0x18 + 4],0xc0000000

	;修改gdt表的虚拟地址
	add dword [gdt_ptr + 2],0xc0000000
  
	add esp,0xc0000000
	
	;将当期页(目录)物理地址放入cr3中
	mov eax,PAGE_DIR_TABLE_POS
	mov cr3,eax

	;开启分页功能
	mov eax,cr0
	or eax,0x80000000
	mov cr0,eax

	;重新加载gdt
	lgdt [gdt_ptr]
	
	jmp SELECTOR_CODE:enter_kernel
enter_kernel:
	call kernel_init		;解析elf格式内核,为运行内核做准备
	mov esp,0xc009f000		;重新设置栈
	jmp KERNEL_ENTRY_POINT	;开始执行os内核

;---- kernel init
kernel_init:
	xor eax,eax
	xor ebx,ebx		;ebx:program header表起始地址
	xor ecx,ecx
	xor edx,edx 

	mov dx,[KERNEL_BIN_BASE_ADDR + 42];program header大小
	mov ebx,[KERNEL_BIN_BASE_ADDR + 28];program header在文件中的偏移量
	add ebx,KERNEL_BIN_BASE_ADDR
	mov cx,[KERNEL_BIN_BASE_ADDR + 44];program header的个数

.each_segment:
	cmp byte [ebx+0],PT_NULL
	je .PTNULL


	push dword [ebx + 16]	;此segment大小
	mov eax,[ebx + 4]		;此segment在文件内的偏移
	add eax,KERNEL_BIN_BASE_ADDR
	push eax
	push dword [ebx+8]		;此段在内存中的虚拟地址
	
	call mem_cpy
	add esp,12				;清除栈参数
.PTNULL:
	add ebx,edx				;跳到下一个program header条目

	loop .each_segment
	ret

;---- mem_cpy函数 栈参数:(dist,src,size)
mem_cpy:
	cld		;清除df方向位,esi/edi + 1
	push ebp
	mov ebp,esp
	push ecx

	mov edi,[ebp + 8]
	mov esi,[ebp + 12]
	mov ecx,[ebp + 16]
	rep movsb 

	pop ecx
	pop ebp
	ret 

;---- 创建页目录和页表
setup_page:
	mov ecx,4096
	mov esi,0
.clear_page_dir:
	mov byte [PAGE_DIR_TABLE_POS + esi],0
	inc esi
	loop .clear_page_dir

	;创建 页目录项
.create_pde:
	mov eax,PAGE_DIR_TABLE_POS
	add eax,0x1000		;+4k 地址大小
	mov ebx,eax			;ebx = 第一个页表的起始物理地址

  or eax,PG_US_U | PG_RW_W | PG_P	          ;4byte的entry加上内存限制属性
  ; or eax,PG_TMP

	mov [PAGE_DIR_TABLE_POS + 0x0],eax	    ;分页开启前后的地址恰好对应
	mov [PAGE_DIR_TABLE_POS + 0xc00],eax    ;虚拟地址的内核空间地址在低1mb中

	sub eax,0x1000
	mov [PAGE_DIR_TABLE_POS + 4092],eax	;最后一项指向页目录自己

	;创建 页表项
	mov ecx,256		;只需要低1mb的空间大小
	mov esi,0
	
  mov edx,PG_US_U | PG_RW_W | PG_P;虚拟地址对应的物理地址从0开始
  ; mov edx,PG_TMP

.create_pte:
	mov [ebx+esi*4],edx
	add edx,4096	;对应的物理地址+4k
	inc esi
	loop .create_pte

	;创建 内核空间的页目录项(pde) : 把内核的虚拟地址空间写死
	mov eax,PAGE_DIR_TABLE_POS
	add eax,0x2000		;第2个页表的物理地址
	
  or eax,PG_US_U | PG_RW_W | PG_P
  ; or eax,PG_TMP
	
  mov ebx,PAGE_DIR_TABLE_POS
	mov ecx,254			;填充254个页目录项
	mov esi,769			;从第769个pde开始填充
.create_kernel_pde:
	mov [ebx+esi*4],eax	;内核空间对应从第2个页表开始
	inc esi
	add eax,0x1000		;
	loop .create_kernel_pde

	ret
	
; ----读取硬盘n个扇区
;eax = LBA扇区号
;ebx = 读入的内存地址
;ecx = 读入的扇区数
rd_disk_m_32:
    mov esi,eax 
    mov di,cx 

;1.设置读取扇区数量
    mov dx,0x1f2
    mov al,cl
    out dx,al

    mov eax,esi 

;2. 设置读取的LBA地址
    mov dx,0x1f3
    out dx,al 
    
    mov cl,8
    shr eax,cl 
    mov dx,0x1f4
    out dx,al

    shr eax,cl
    mov dx,0x1f5
    out dx,al

    shr eax,cl 
    and al,0x0f
    or al,0xe0 ;lba+主盘+lab 23-27bit
    mov dx,0x1f6
    out dx,al

;3. command
    mov dx,0x1f7
    mov al,0x20
    out dx,al

;4. 检测硬盘状态
.not_ready:
    nop
    in al,dx ;从status端口读取状态
    and al,0x88
    cmp al,0x08
    jnz .not_ready

;5. 从data端口读取数据
    mov ax,di
    mov dx,256
    mul dx
    mov cx,ax

    mov dx,0x1f0
.go_on_read:
    in ax,dx
    mov [ebx],ax
    add ebx,2
    loop .go_on_read
    
    ret
