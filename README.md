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



### 2021-6-11: 修改font.c

1. 将font_vaddr()做成局部,最后一个参数和putfont8_str()一样
2. 修改putfont8_int(),底层调用sprintf(),支持显示负数



### 2021-6-13:  接收鼠标数据

1. 用30天作者的fifo8数据结构接收数据失败,是因为没有考虑到内核线程互斥的问题,还是使用elephant-os已经有的ioqueue(也是fifo结构)解决互斥问题



### 2021-6-14:

> 所有用户进程的优先级是31

1. 添加内核线程`k_graphics`,因为目前是在init()中绘制出来的图,让一个内核线程单独来画图,放在main()中开始
2. 添加专门解析鼠标数据的内核线程`k_g_mouse`
3. 鼠标坐标初始化,根据数据移动鼠标



### 2022-1-6:修改mouse,添加图层api

1. 已经换掉了鼠标中断接收数据的带锁的ioqueue(ring buffer),用上不带锁的ring buffer加速

2. sheetctl_init的malloc_page好像没有加锁

   替换malloc_page,用sys_malloc()来申请内存

### 2022-1-9:添加图层

1. 启动图层,添加鼠标和桌面两个图层

2. 因为图层的原因,又把解析鼠标数据保持和书上大致一样,(不单独开一个内核线程解析数据).将解析鼠标数据的操作放到kernel_graphics线程中,

   #### 鼠标绘图逻辑:

   1. decode更新鼠标的新坐标后,直接修改sht_mouse的坐标就行了,然后sheet_refresh(),合并成了一个sheet_slide()操作

3. 替换所有原始操作vram,换成填充sheet,然后刷新sheet

4. 删除 graphics/include/paint.h graphics/paint.c


### 2022-1-13: 添加图层加速处理(局部刷新)

1. 修改sheet_refresh(),添加sheet_refreshsub().

1. 实现鼠标在画面外
2. 修改sheet_xx()函数的接口
3. 添加窗口windows

### 2022-1-14: 绘制窗口的删除btn+解决图层闪烁问题

1. 在标题栏添加 删除按钮(类似绘制鼠标一样)
2. 窗口闪烁的原因: (当前图层下面图层刷新导致)

   * refreshsub虽然只刷新给出的屏幕绝对区域,但是是从最下面的图层开始刷新的,只要一直刷新(h>0)的图层,会一直导致闪烁.
   * 解决办法: 因为h=0没有改变也要重新绘制,所以只要将待刷新的图层下面的图层不刷新即可,所有待刷新图层上的图层都要刷新.修改sheet_refreshsub()函数.

3. 鼠标闪烁的问题:(当前图层上面的图层刷新导致)

   * 当前图层所在的区域(部分区域)上有更高的图层,因为重新绘制当前图层又会被高图层绘制后覆盖,所以如果部分区域上有更高的图层,那么这部分区域可以不绘制.
   * 解决办法: 添加一个320*200的内存来保存屏幕每一个节点的最高的图层,这样可以避免当前图层刷新后完,上面的图层又再次在相同地方刷新,添加ctl.map,sheet_refreshmap(),注意map的作用
4. 修改的sheet_refreshsub()的参数,刷新的图层最高高度需要传入.





## 常用的源文件

1. 有关线程初始化全在thread/thread.c thread_init()中





## 修复的问题

1. 2021-4-25:修复在Loader.s下写入0xff0的screen的信息,
2. 2022-1-14: 在k_mouse()中添加了一个写buf_win(定时器窗口)的操作,结果鼠标不飞了((离谱))



## 任务/bug

1. graphics子模块当前运行在main()中,也就是内核主线程中,后期需要从内核主线程中剥离出去
2. 将图形操作不再在main.c中测试,而是创建一个内核线程来执行图形操作(未实现)



## 资料

[调色板palette](http://www.360doc.com/content/10/0928/15/2790922_57060786.shtml)

[^abc]:http://baidu.com

