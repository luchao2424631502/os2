## 环境配置

> X86_64 Linux下:
>
> 1. make
>
> 2. nasm
> 3. gcc
> 4. ld
> 5. bochs ,配置文件可用./bochsrc
>
> 用bximage创建好虚拟硬盘(可自行变更) `bximage`,注意修改makefile中的创建好的虚拟硬盘名称

## 分支情况

 									head

​										|

​									master

​										|

​								[实现系统调用]

[实现用户进程]->

​								[vga]-->[desktop]-->[mouse]

​																		|

​																	graphics



## 编译运行

`make all`,得到elf格式的`kernel.bin`

`bochs`运行.(配置文件在当前目录下)

`make clean`:清除编译产生的中间文件

## 目前想法是在实现file system后会将图形界面引入进来(2个分支)



## 系统调用 2021-4-28添加系统调用

### 系统调用实现思路

1. IDT中安装0x80的中断描述符,注册0x80的中断处理例程(DPL=3)
2. 在内核中实现每一个系统调用,入口用syscall_table数组组织,ring 3通过eax的值作为syscall_table的索引进入具体的系统调用
3. ring 3的用户空间用宏实现(封装)进入0x80中断的指令和传递的参数

### 添加系统调用的步骤

1. 向`syscall.h`中enum syscall_nr结构中添加新的子功能号,和用户空间函数声明
2. 在`syscall.c`中实现用户函数接口(调用_syscallx()宏)
3. 在`syscall-init.c`中实现调用函数,并且向`syscall_table`中注册

## 添加printf功能 2021-4-30

> 测试用户进程(ring 3)下都是靠内核线程来帮助打印,现在添加ring 3的打印功能

1. 添加了简单的write系统调用
2. 添加vsprintf()和itoa()函数
3. 添加printf()

## 完善堆内存管理 2021-5-1

使用arena结构管理小的内存块分配,支持7种大小的内存块,`16 32 64 128 256 412 1024`

#### 添加mem_block,mem_block_desc,arena结构,

> 内核空间 内存块描述符数组 定义在memory.c
>
> 用户空间 内存块描述符数组 定义在pcb中

 #### 实现sys_malloc,(实现用户进程的堆内存管理)

> 修改thread.h的pcb结构
>
> 在process.c中添加u_block_descs[]的初始化
>
> 添加2个地址转换函数:arena2block(),block2arena().

最后从mem_block_desc.free_list中找到了mem_block,就会将mem_block填充为0,

#### 内存释放 2021-5-2

> 回收内存指的是将bitmap清零,并不是真正的将memory的字节清零

内存分配实际调用函数[^malloc_page()],内存释放按照malloc_page()注释中的相反步骤来做.

mfree_page(粒度:页)流程:

1. pfree()释放物理页
2. page_table_pte_remove():在page_table中去掉映射关系
3. vaddr_remove():在虚拟内存池中释放虚拟地址对应的页(清除bitmap)

添加sys_free()后测试时,在page_table_add()处产生错误,来源于pfree()等函数中变量写错,目前sys_free()测试成功

#### 添加系统调用malloc()和free() 2021-5-4

## 编写硬盘驱动程序 2021-5-7

1. 打开irq2+硬盘中断
2. 添加printk()

...

## 文件系统开始 2021-5-9

1. 5-12完成创建文件系统,

2. 5-18实现sys_open()中的文件创建

3. 5-18实现sys_open()文件的打开,和文件的关闭

4. 5-19实现文件写入sys_write()

5. 5-19实现文件读取sys_read()

6. 5-20实现文件定位sys_lseek()

7. 5-20实现文件删除sys_unlink()

8. 5-20实现创建目录sys_mkdir()

9. 5-20实现遍历目录-1.打开和关闭目录 sys_opendir(),sys_closedir()-2.sys_readdir()和sys_rewinddir()

   > fs/fs.c sys_opendir(): line 653 判断应该有问题(待修改)

10. 5-21实现删除目录sys_rmdir()
11. 5-21实现获得当前工作目录sys_getcwd(),和切换工作目录sys_chdir()
12. 5-21实现读取文件属性sys_stat()



## IPC 2021-5-21

1. 5-22 sys_fork()的实现,添加fork()系统调用
2. 实现init进程,由loader->init进程

## 以下是写的过程中的一些tips

C:表示一致性代码段,也称为依从代码段,Conforming,一致性代码段是指如果自己是转移的目标段,并且自己是一致性代码段,自己的特权级一定要高于当前特权级,转移后的特权级不与自己的DPL为主,而是与转移前的低特权级一致,(依从)



#### 进入保护模式后可重新加载GDTR,实现gdt表放在高于1mb的地址

处理器微架构表示:

```
mov eax,[0x1234]

push eax ;sub esp,4
		  mov [esp],eax

call function
```

----

## 内存部分

### 大致的内存布局(物理内存):

#### 执行的流程:

> 0x7c00(mbr)->(0xb00=0x900+0x300)(loader)->0x1500(kernel_entry_point)

#### 1mb下:

> elf内核加载到`0x70000`处,
>
> 解析elf内核时,因为编译指定了kernel的起始地址为0xc0001500,所以将data段加载到物理地址0x1500处

1. 内存管理的bitmap放在`0x9a000`处(包括vaddr,和physical addr)
2. `loader.s`:进入内核前设置前内核栈顶为`0x9f000`,所以内核线程`pcb`地址是`0x9e000`

#### 1mb上:

> 0x100000处页目录的第769开始的页目录项目指向从0x102000开始的页表,这对应着1GB虚拟内存空间,

1. 所以1mb上2+254个页表占用的1mb的物理内存空间,真正可用物理内存从0x200000开始分配



#### 启用分页机制的顺序

1. 准备好页目录和页表
2. 将页表地址写入cr3,(cr3:页目录基址寄存器)
3. cr0的pg = 1(paging = 1),控制寄存器开启分页

> 填写了3个页目录项目,0指向1mb,0xc00指向高3gb的内核的虚拟地址空间,4092指向页目录自己

#### 大象内核在低1mb处,mbr+loader+kernel

开启分页后gdt表虚拟地址和video描述符的基地址都是在kernel部分 3gb后,实际在1mb下

#### 低1mb物理地址和虚拟地址的1对1映射

```
真实物理地址: = 0x80000
0000 0000 00
0010 0000 00
0000 0000 0000

分页机制下:
0=第0个页目录项
2^7=128

128*4k = 0x80000
开启分页机制后刚好映射到低1mb的地址空间
```

#### 当前映射下获取 页目录项 或 页表项 的方法

1. 0xfffffxxx,前20bit索引到最后是**页目录**的基地址,xxx作为偏移则是 项的索引*4
2. 高10bit: 0x3ff,索引到页目录自身,中间10bit定位具体页表,12bit偏移=页表项索引值*4

### TLB(translation lookaside buffer)介绍

用来存放虚拟地址页框与物理地址页框的映射关系,

当页表中的内容修改后,要人为的更新tlb,保证内存访问的正确

1. 重新加载cr3寄存器,
2. invlpg指令: 格式invlpg x (invalidate page),x是虚拟地址,用来在tlb检索

### elf内核加载地址

elf内核放在高地址(0x70000),等解析后的内核镜像在低地址运行起来后就可以覆盖高地址elf格式内核(没用处了)

### 内核镜像运行地址:

在加载内核后和开启分页后,要分析加载elf格式的内核,从Loader正式进入kernel中,**loader中,在进入kernel_entry_point前将内核的栈`esp`设置为1mb下的高地址`0x9f000`



## 栈

1. loader使用的栈是0x900以下,loader在0x900处
2. loader开启分页跳到kernel时重新设置的栈是`0xc009f000`,物理地址也就是`0x9f000`,接近1mb的最高处

### PCB(进程控制块)

pcb大小是4kb,并且只能单独占用一个1页框,pcb的开始,地址`0xabcde000`开始存放的进程的信息状态等,**pcb的最高处`0xabcdefff`以下的内存作为进程或者线程在0特权级下所使用的栈,所以安排内核的栈顶是`0xc009f000`,则pcb的起始地址是`0xc009e000`**



## 内核线程实现



## 用户进程实现

### 虚拟地址空间管理

### 创建页表和ring 3 stack

> 2021/4/15: 修改了部分memory.c 

### 如何进入ring3?

1. 调用门返回到ring3
2. 中断返回到ring3

### 从中断返回ring3的流程

1. 提前准备好用户进程所用的栈结构,填好用户进程的上下文信息
2. `cs.rpl`等于当前cpu的特权级,所以`cs.rpl=3`
3. 栈中段寄存器的选择子必须指向`dpl=3`的描述符(内存段)
4. 退出中断后,继续允许中断`eflags.if = 1`
5. 用户进程不能直接io,访问硬件设备,`eflags.iopl = 0`

#### bss

大概3种属性的section:

1. 可读可写的数据,如 数据节`.data` 和未初始化节 `.bss`
2. 只读可执行的代码,如  代码节 `.text` 和 初始化代码节`.init` 
3. 只读数据,如`.rodata`

### ring 3的用户进程能够访问内核空间的数据的原因

1. 进程自己的页目录表的高1GB和内核页目录表相同,
2. 并且这些页目录项和页表项的权限=PG_US_U,即**所有特权级的代码都可以访问,反之,如果页目录项/页表项此bit为PG_US_S,则ring 3用户进程访问不了内核空间的数据**











-------

### hd60M.img虚拟磁盘使用情况

1. 0扇区mbr
2. 1扇区没用
3. loader写入第2个扇区,并且占用4个扇区大小 (2-5)
4. elf格式的内核文件写入第10个扇区(扇区9)

------

### 编译事项

1. -Ttext指定链接生成的文件中代码段起始的虚拟地址空间
2. -e 指定进程的入口符号(函数)
3. ld: -m elf_i386指定链接成x86架构
4. gcc:-m32在x64下编译成32bit代码

#### 5. 栈保护,"__stack_chk_fail"连接失败原因:

​	gcc编译默认开启了栈保护,会调用`__stack_chk_fail`函数,而我们没有链接C标准库,所以报错

​	`-fno-stack-protector`关闭gcc栈保护,就行

### 汇编注意事项:

1. call调用时的栈的变化

   当提前通过栈传递参数时,call调用会把pc下一条地址push入栈,所以注意此时栈顶是call指令的下一条地址,



[^malloc_page()]:https://github.com/luchao2424631502/os2/blob/master/kernel/memory.c

