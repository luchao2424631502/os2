## os2上graphics分支开发日志(2021-4-20) 

### 目标和当前情况

1. 在master的`支持用户进程`的commit节点创建graphics分支,引入一些图形操作

2. 6-8在graphics节点下merge


 ### 2021-4-24:将30天中的txt文件ascii字体像素数据转化为(8bits*16)byte array放入elf文件中

 [blog]([http://luchao.wiki/2021/04/24/%E5%B0%8630%E5%A4%A9%E4%B8%ADtxt%E6%96%87%E4%BB%B6%E7%9A%84ascii%E5%AD%97%E4%BD%93%E5%83%8F%E7%B4%A0%E6%95%B0%E6%8D%AE%E8%BD%AC%E4%B8%BAbinary%E6%8F%92%E5%85%A5elf%E6%A0%BC%E5%BC%8F%E7%9A%84kernel%E4%B8%AD/](http://luchao.wiki/2021/04/24/将30天中txt文件的ascii字体像素数据转为binary插入elf格式的kernel中/))

### 2021-4-26:添加ps/2鼠标中断 

[blog]([http://luchao.wiki/2021/04/30/X86%E4%B8%8B%E6%8E%A7%E5%88%B6Intel8042%E5%88%9D%E5%A7%8B%E5%8C%96%E9%BC%A0%E6%A0%87/](http://luchao.wiki/2021/04/30/X86下控制Intel8042初始化鼠标/))

修改bochsrc配置文件,开启鼠标(default是ps/2)`mouse: enable`

> 添加后在bochs下`shwo extint`没有看到鼠标中断,**需要对鼠标电路初始化**

向intel 8042发送流程:

```c
//向命令端口0x64发送命令0x60:写入数据端口0x60的字节将写入8042芯片的控制寄存器
//=(设置8042的工作模式)
outb(0x64,0x60); 
outb(0x60,0x47);


//向命令端口0x64发送命令0xd4:写入数据端口0x60的字节将发送给ps/2鼠标芯片
outb(0x64,0xd4);
//0xf4:使能鼠标的数据报告功能并复位它的位移算数器这条命令只对Stream模式下的数据报告科效=(打开数据传送)
outb(0x60,0xf4);
```

> bug:必须同时开启键盘中断+irq2+ps/2鼠标中断,如果只单独开启键盘中断,键盘输入后键盘中断没发生

> intel 8042作为intel 键盘芯片 8048的代理,位于南桥中



### 2021-4-25:在elepant已有键盘驱动和ioqueue(环形输入缓冲区)中在图形下测试打字 

在键盘驱动keyboard.c中添加graphics操作就行



### 2021-6-8:在graphics分支下将master分支(elephant-os已经实现shell的情况下)merge过来,解决一些冲突,如main.c,init.c,interrupt.c,

目的:在elephant-os的基础上开发,可以直接用线程进程的创建和消除以及一些从当初没有的api

> 修改Makefile,支持make bflags=-dmbr all编译带图形,(避免了每次修改显示模式)
>
> make all不带图形,



## 修复的问题

1. 2021-4-25:修复在Loader.s下写入0xff0的screen的信息,



## 任务/bug

1. graphics子模块当前运行在main()中,也就是内核主线程中,后期需要从内核主线程中剥离出去
2. 将图形操作不再在main.c中测试,而是创建一个内核线程来执行图形操作(未实现)



## 资料

[调色板palette](http://www.360doc.com/content/10/0928/15/2790922_57060786.shtml)

[^abc]:http://baidu.com

