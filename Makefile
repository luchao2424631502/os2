BUILD_DIR  =./tmp
ENTRY_POINT=0xc0001500
AS				 =nasm
CC  			 =gcc
LD 				 =ld
INCLUDE		 =-I lib/ -I lib/kernel/ -I lib/user/ -I kernel/ -I device/ -I thread/ -I userprog/ -I fs/
ASFLAGS    =-f elf
CFLAGS     =-Wall $(INCLUDE) -c -m32 -fno-builtin -fno-stack-protector -W -Wno-implicit-fallthrough#-Wmissing-prototypes -Wstrict-prototypes
#Wall 和 W 是警告信息,Wstrict-prototypes:函数声明需要指出参数类型  Wmissing-prototypes:没有预先声明函数旧定义全局函数将警告
LDFLAGS    =-Ttext $(ENTRY_POINT) -e main -m elf_i386 -Map $(BUILD_DIR)/kernel.map
OBJS       =$(BUILD_DIR)/main.o $(BUILD_DIR)/init.o $(BUILD_DIR)/interrupt.o $(BUILD_DIR)/timer.o\
						$(BUILD_DIR)/debug.o $(BUILD_DIR)/string.o $(BUILD_DIR)/bitmap.o $(BUILD_DIR)/memory.o\
						$(BUILD_DIR)/thread.o $(BUILD_DIR)/list.o\
						$(BUILD_DIR)/kernel.o $(BUILD_DIR)/print.o $(BUILD_DIR)/switch.o $(BUILD_DIR)/sync.o\
						$(BUILD_DIR)/console.o $(BUILD_DIR)/keyboard.o $(BUILD_DIR)/ioqueue.o\
						$(BUILD_DIR)/tss.o $(BUILD_DIR)/process.o $(BUILD_DIR)/syscall.o\
						$(BUILD_DIR)/syscall-init.o $(BUILD_DIR)/stdio.o $(BUILD_DIR)/stdio-kernel.o\
						$(BUILD_DIR)/ide.o $(BUILD_DIR)/fs.o $(BUILD_DIR)/inode.o\
						$(BUILD_DIR)/file.o $(BUILD_DIR)/dir.o $(BUILD_DIR)/fork.o
BOOTLOADER = $(BUILD_DIR)/mbr.bin $(BUILD_DIR)/loader.bin
# ---- boot + loader ----
$(BUILD_DIR)/mbr.bin: boot/mbr.s
	$(AS) -Iboot/include/ $< -o $@

$(BUILD_DIR)/loader.bin: boot/loader.s
	$(AS) -Iboot/include/ $< -o $@
# ---- C文件编译 -----
$(BUILD_DIR)/main.o:kernel/main.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/init.o:kernel/init.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/interrupt.o:kernel/interrupt.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/timer.o:device/timer.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/debug.o:kernel/debug.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/string.o:lib/string.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/bitmap.o:lib/kernel/bitmap.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/memory.o:kernel/memory.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/thread.o:thread/thread.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/list.o:lib/kernel/list.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/sync.o:thread/sync.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/console.o:device/console.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/keyboard.o:device/keyboard.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/ioqueue.o:device/ioqueue.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/tss.o:userprog/tss.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/process.o:userprog/process.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/syscall.o:lib/user/syscall.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/syscall-init.o:userprog/syscall-init.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/stdio.o:lib/stdio.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/stdio-kernel.o:lib/kernel/stdio-kernel.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/ide.o:device/ide.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/fs.o:fs/fs.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/inode.o:fs/inode.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/file.o:fs/file.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/dir.o:fs/dir.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/fork.o:userprog/fork.c
	$(CC) $(CFLAGS) $< -o $@

#---- ASM汇编文件编译 ----
$(BUILD_DIR)/kernel.o:kernel/kernel.s
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/print.o:lib/kernel/print.s
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/switch.o:thread/switch.s
	$(AS) $(ASFLAGS) $< -o $@

#---- 链接所有的目标文件生成OS内存镜像 ----
$(BUILD_DIR)/kernel.bin:$(OBJS)
	$(LD) $(LDFLAGS) $^ -o $@

#伪目标
.PHONY:bootloader hd clean all

#如果$(BUILD_DIR)不存在则创建
#make_dir:
#	if [[ ! -d $(BUILD_DIR) ]];then mkdir $(BUILD_DIR);fi
bootloader: $(BOOTLOADER)
	dd if=$(BUILD_DIR)/mbr.bin of=/home/luchao/Work/os2/hd60M.img bs=512 count=1 seek=0 conv=notrunc
	dd if=$(BUILD_DIR)/loader.bin of=/home/luchao/Work/os2/hd60M.img bs=512 count=4 seek=2 conv=notrunc

hd:
	dd if=$(BUILD_DIR)/kernel.bin \
		of=/home/luchao/Work/os2/hd60M.img\
		bs=512 seek=9 count=200 conv=notrunc

clean:
	cd $(BUILD_DIR) && rm -f ./*

build:$(BUILD_DIR)/kernel.bin

all:bootloader build hd
