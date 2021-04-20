%include "boot.inc"

SECTION MBR vstart=0x7c00
    mov ax,cs 
    mov ds,ax 
    mov es,ax 
    mov ss,ax 
    mov fs,ax
    mov sp,0x7c00
    mov ax,0xb800
    mov gs,ax

; INT 0x10 功能号:0x06 作用:上卷窗口
; ah = 0x06
; al = 上卷行数（ 0表示全部 ) 
; BH = 上卷行属性
; (CL,CH) = 窗口左上角(X,Y)位置
; (DL,DH) = 窗口右下角(X,Y)位置
    mov ax,0600h
    mov bx,0700h
    mov cx,0
    mov dx,184fh
    int 10h


;输出字符串 1 MBR
    mov byte [gs:0x00],'1'
    mov byte [gs:0x01],0xA4 ; 1010 0100

    mov byte [gs:0x02],' '
    mov byte [gs:0x03],0xA4

    mov byte [gs:0x04],'M'
    mov byte [gs:0x05],0xA4

    mov byte [gs:0x06],'B'
    mov byte [gs:0x07],0xA4

    mov byte [gs:0x08],'R'
    mov byte [gs:0x09],0xA4

;graphics:------ 
;定义保存数据的位置
CYLS  equ   0x0ff0  ;启动区
LEDS  equ   0x0ff1  ;键盘LED状态
VMODE equ   0x0ff2  ;颜色数目信息
SCRNX equ   0x0ff4  ;分辨率 x
SCRNY equ   0x0ff6  ;分辨率 y
VRAM  equ   0x0ff8  ;图像缓冲区开始的地址
;开启VGA模式
    ; mov al,0x13     ;VGA 320*200*8
    ; mov ah,0x00
    ; int 0x10
;graphics:------

;
    mov eax,LOADER_START_SECTOR
    mov bx,LOADER_BASE_ADDR
    mov cx,4    ;由于loader变大,读取4个扇区
    call rd_disk_m_16

    jmp LOADER_BASE_ADDR + 0x300
;读取硬盘n个扇区
;eax = LBA扇区号
;ebx = 读入的内存地址
;ecx = 读入的扇区数
rd_disk_m_16:
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
    mov [bx],ax
    add bx,2
    loop .go_on_read
    
    ret

times 510 - ($-$$) db 0
db 0x55,0xaa


