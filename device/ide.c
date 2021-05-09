#include "ide.h"
#include "stdio-kernel.h"
#include "global.h"
#include "stdio.h"
#include "sync.h"
#include "debug.h"
#include "io.h"
#include "timer.h"
#include "interrupt.h"
#include "list.h"
#include "memory.h"
#include "string.h"

/*硬盘寄存器端口
 * 通道1:命令寄存器:0x1F0+(0-7),控制寄存器:0x3F6
 * 通道2:命令寄存器:0x170+(0-7),控制寄存器:0x376
 * */
#define reg_data(channel)       (channel->port_base + 0)
#define reg_error(channel)      (channel->port_base + 1)
#define reg_sect_cnt(channel)   (channel->port_base + 2)
#define reg_lba_l(channel)      (channel->port_base + 3)
#define reg_lba_m(channel)      (channel->port_base + 4)
#define reg_lba_h(channel)      (channel->port_base + 5)
#define reg_dev(channel)     (channel->port_base + 6)

/*读是status,写是command*/
#define reg_status(channel)     (channel->port_base + 7)
#define reg_cmd(channel)        reg_status(channel)

/*control block regists*/
#define reg_alt_status(channel) (channel->port_base + 0x206)
#define reg_ctl(channel)        reg_alt_status(channel)

/*0x1F7 寄存器 Status状态*/
#define BIT_STAT_BSY  0x80      //1=硬盘正忙
#define BIT_STAT_DRDY 0x40      //1=设备就绪,等待指令
#define BIT_STAT_DRQ  0x8       //1=硬盘已经准备好数据

/*0x1F6 Device寄存器 */
#define BIT_DEV_MBS   0xa0      //寄存器值固定
#define BIT_DEV_LBA   0x40      //1=LBA寻址方式 0=CHS
#define BIT_DEV_DEV   0x10      //0=主盘,1=从盘

/*0x1F7 寄存器 Command状态*/
#define CMD_IDENTIFY  0xec      //硬盘识别
#define CMD_READ_SECTOR 0x20    //读扇区
#define CMD_WRITE_SECTOR  0x30  //写扇区

#define MAX_LBA ((80 * 1024 * 1024 / 512) - 1) //80MB的扇区个数-1 = index range
#define max_lba MAX_LBA

uint8_t channel_cnt;            //ata通道个数
struct ide_channel channels[2]; //一个ata通道2个硬盘 

int32_t ext_lba_base = 0;       //总扩展分区的起始lba
uint8_t p_no = 0,l_no = 0;      //主分区和逻辑分区的下标
struct list partition_list;     //分区队列

/*存分区表项*/
struct partition_table_entry
{
  uint8_t bootable;
  uint8_t start_head;   //起始磁头号
  uint8_t start_sec;    //起始扇区号
  uint8_t start_chs;    //起始柱面号
  uint8_t fs_type;      //分区类型id
  uint8_t end_head;     //结束磁头号
  uint8_t end_sec;      //结束扇区号
  uint8_t end_chs;      //结束柱面号

  uint32_t start_lba;   //分区起始lba  
  uint32_t sec_cnt;     //分区扇区数量
} __attribute__((packed));

/*mbr引导扇区内容*/
struct boot_sector
{
  uint8_t other[446];   //mbr引导内容
  struct partition_table_entry partition_table[4];
  uint16_t signature;   //0x55,0xaa
}__attribute__((packed));

/*向reg_dev端口写入选择的硬盘*/
static void select_disk(struct disk *hd)
{
  uint8_t reg_device = BIT_DEV_MBS | BIT_DEV_LBA;
  //从盘
  if (hd->dev_no == 1)
  {
    reg_device |= BIT_DEV_DEV;
  }
  outb(reg_dev(hd->my_channel),reg_device);
}

/*写入Lba28和读写的扇区数*/
static void select_sector(struct disk *hd,uint32_t lba,uint8_t sec_cnt)
{
  ASSERT(lba <= MAX_LBA);
  struct ide_channel *channel = hd->my_channel;

  //写入扇区数
  outb(reg_sect_cnt(channel),sec_cnt);

  //写入lba 8*3=24
  outb(reg_lba_l(channel),lba);
  outb(reg_lba_m(channel),(lba >> 8));
  outb(reg_lba_h(channel),(lba >> 16));

  //lba 24-27
  outb(reg_dev(channel),BIT_DEV_MBS | BIT_DEV_LBA | (hd->dev_no == 1 ? BIT_DEV_DEV : 0) | lba >> 24);
}

/*向channel发出command*/
static void cmd_out(struct ide_channel *channel,uint8_t cmd)
{
  channel->expecting_intr = true;
  outb(reg_cmd(channel),cmd);
}

/*从硬盘中读取sec_cnt个扇区数据到buf中*/
static void read_from_sector(struct disk *hd,void *buf,uint8_t sec_cnt)
{
  uint32_t size_in_byte;
  if (sec_cnt == 0)
  {
    size_in_byte = 256 * 512;
  }
  else
  {
    size_in_byte = sec_cnt * 512;
  }
  insw(reg_data(hd->my_channel),buf,size_in_byte/2);
}

/*将buf中的sec_cnt个扇区数据写入disk中*/
static void write2sector(struct disk *hd,void *buf,uint8_t sec_cnt)
{
  uint32_t size_in_byte;
  if (sec_cnt == 0)
  {
    size_in_byte = 256 * 512;
  }
  else 
  {
    size_in_byte = sec_cnt * 512;
  }
  outsw(reg_data(hd->my_channel),buf,size_in_byte / 2);
}

//非busy状态下返回1=数据准备好了,0=出错
static bool busy_wait(struct disk *hd)
{
  struct ide_channel *channel = hd->my_channel;
  int16_t time_limit = 30 * 1000;
  do {
    time_limit -= 10;
    //硬盘不是busy状态
    if (!(inb(reg_status(channel)) & BIT_STAT_BSY))
    {
      //返回hd是否准备好了数据
      return (inb(reg_status(channel)) & BIT_STAT_DRQ);
    }
    //硬盘正忙
    else
    {
      mtime_sleep(10);//睡眠10ms
    }
  } while (time_limit >= 0);
  return false;
}

/*完整的从硬盘读取sec_cnt个扇区到buf
 * lba:硬盘扇区起始地址
 * buf:内存buf地址
 * sec_cnt:读取扇区个数
 * */
void ide_read(struct disk *hd,uint32_t lba,void *buf,uint32_t sec_cnt)
{
  ASSERT(lba <= MAX_LBA);
  ASSERT(sec_cnt > 0);
  lock_acquire(&hd->my_channel->lock);

  /*1.选择hd*/
  select_disk(hd);

  uint32_t secs_op;
  uint32_t secs_done = 0;
  /*控制器一次最多操作256个扇区,如果sec_cnt>256要分多次来*/
  while (secs_done < sec_cnt)
  {
    /*判断剩下需要读的扇区数是否>256*/
    if ((secs_done + 256) <= sec_cnt)
    {
      secs_op = 256;
    }
    else 
    {
      secs_op = sec_cnt - secs_done;
    }

    /*2.写入Lba + sec_cnt*/
    select_sector(hd,lba+secs_done,secs_op);

    /*3.将读命令写入command寄存器*/
    cmd_out(hd->my_channel,CMD_READ_SECTOR);

    /*将自己(硬盘驱动线程)换下cpu,因为此时硬盘在工作,等硬盘产生中断,通过schedule上cpu执行*/
    sema_down(&hd->my_channel->disk_done);

    /*4.(执行到这里说明硬盘可读)检测硬盘现在是否可读*/
    if (!busy_wait(hd))
    {
      char error[64];
      sprintf(error,"%s read sector %d failed",hd->name,lba);
      PANIC(error);
    }
    
    /*5.正式从硬盘将数据读取到buf中*/
    read_from_sector(hd,(void*)((uint32_t)buf + secs_done * 512),secs_op);

    secs_done += secs_op;
  }
  lock_release(&hd->my_channel->lock);
}

/*将buf处的sec_cnt个扇区写入硬盘*/
void ide_write(struct disk *hd,uint32_t lba,void *buf,uint32_t sec_cnt)
{
  ASSERT(lba <= MAX_LBA);
  ASSERT(sec_cnt > 0);
  lock_acquire(&hd->my_channel->lock);

  select_disk(hd);

  uint32_t secs_op;
  uint32_t secs_done = 0;
  while (secs_done < sec_cnt)
  {
    if ((secs_done + 256) <= sec_cnt)
    {
      secs_op = 256;
    }
    else 
    {
      secs_op = sec_cnt - secs_done;
    }

    select_sector(hd,lba+secs_done,secs_op);

    cmd_out(hd->my_channel,CMD_WRITE_SECTOR);
    
    if (!busy_wait(hd))
    {
      char error[64];
      sprintf(error,"%s write sector %d failed\n",hd->name,lba);
      PANIC(error);
    }

    write2sector(hd,(void*)((uint32_t)buf + secs_done * 512),secs_op);

    /*在硬盘执行写时, 硬盘驱动(自己)程序需要换下cpu让其它进程执行*/
    sema_down(&hd->my_channel->disk_done);

    secs_done += secs_op;
  }
  lock_release(&hd->my_channel->lock);
}

/**/
static void swap_pairs_bytes(const char *dst,char *buf,uint32_t len)
{
  uint8_t idx;
  for (idx=0; idx<len; idx+=2)
  {
    buf[idx + 1] = *dst++;
    buf[idx] = *dst++;
  }
  buf[idx] = '\0';
}

/*获得硬盘参数信息*/
static void identify_disk(struct disk *hd)
{
  char id_info[512];
  select_disk(hd);

  cmd_out(hd->my_channel,CMD_IDENTIFY);

  //目前磁盘在工作,将硬盘驱动进程换下cpu
  sema_down(&hd->my_channel->disk_done);

  if (!busy_wait(hd))
  {
    char error[64];
    sprintf(error,"%s identify failed\n",hd->name);
    PANIC(error);
  }
  read_from_sector(hd,id_info,1);

  char buf[64];
  uint8_t sn_start = 10*2;    //硬盘序列号起始byte,长度为20字节
  uint8_t sn_len = 20;
  uint8_t md_start = 27*2;    //硬盘型号,长度40byte
  uint8_t md_len = 40;
  
  swap_pairs_bytes(&id_info[sn_start],buf,sn_len);
  printk("    [Disk] %s info:\n",hd->name);
  printk("    SN: %s\n",buf);
  memset(buf,0,sizeof(buf));

  swap_pairs_bytes(&id_info[md_start],buf,md_len);
  printk("    Module: %s\n",buf);

  uint32_t sectors = *(uint32_t *)&id_info[60*2];        //供用户使用的扇区数量
  printk("    Sectors: %d\n",sectors);
  printk("    Capacity: %dMb\n",sectors*512/1024/1024);
}

/**/
static void partition_scan(struct disk *hd,uint32_t ext_lba)
{
  struct boot_sector *bs = sys_malloc(sizeof(struct boot_sector));
  ide_read(hd,ext_lba,bs,1);
  uint8_t part_idx = 0;
  //mbr中分区表数组
  struct partition_table_entry *p = bs->partition_table;

  /*遍历最多4个主分区*/
  while (part_idx++ < 4) 
  {
    //扩展分区
    if (p->fs_type == 0x5)
    {
      //不是首次scan,则总扩展分区的lba已经找到
      if (ext_lba_base != 0)
      {
        //在子扩展分区的ebr的分区表项中递归调用,(因为是链表形式
        partition_scan(hd,p->start_lba + ext_lba_base);
      }
      //=0说明是首次scan
      else 
      {
        //记录总扩展分区的起始lba,后面子扩展分区以此为base
        ext_lba_base = p->start_lba;
        partition_scan(hd,p->start_lba);
      }
    }
    //其它有效分区类型
    else if (p->fs_type != 0)
    {
      //主分区
      if (ext_lba == 0)
      {
        //在struct hd中的自定义的partition结构中记录分区的信息
        hd->prim_parts[p_no].start_lba = ext_lba + p->start_lba;
        hd->prim_parts[p_no].sec_cnt = p->sec_cnt;
        hd->prim_parts[p_no].my_disk = hd;    //分区所属的硬盘
        list_append(&partition_list,&hd->prim_parts[p_no].part_tag); //加入到全局变量partition_list记录所有分区
        sprintf(hd->prim_parts[p_no].name,"%s%d",hd->name,p_no+1);   //sdb[1-4]
        printk("name = %s\n",hd->prim_parts[p_no].name);
        p_no++;
        ASSERT(p_no<4); // 0 1 2 3
      }
      //子扩展分区中的普通分区
      else 
      {
        hd->logic_parts[l_no].start_lba = ext_lba + p->start_lba;
        hd->logic_parts[l_no].sec_cnt = p->sec_cnt;
        hd->logic_parts[l_no].my_disk = hd;
        list_append(&partition_list,&hd->logic_parts[l_no].part_tag);
        sprintf(hd->logic_parts[l_no].name,"%s%d",hd->name,l_no + 5);//logic分区号从5开始
        printk("name = %s\n",hd->logic_parts[l_no].name);
        l_no++;
        /*理论上逻辑分区的个数是无限的,这里最多只支持8个逻辑分区*/
        if (l_no >= 8)
          return ;
      }
    }

    //指向mbr扇区的下一个分区表项
    p++;
  }
  sys_free(bs);
}

/*打印分区信息*/
static bool partition_info(struct list_elem *elem,int arg UNUSED)
{
  //根据成员得到struct的首地址
  struct partition *part = elem2entry(struct partition,part_tag,elem);
  printk("list_traversal_part_name: %s",part->name);
  printk("    %s start_lba:0x%x,sec_cnt:0x%x\n",part->name,part->start_lba,part->sec_cnt);

  //让list_traversal()中的if()不执行,就可以遍历所有
  return false;
}

/*硬盘中断处理程序*/
void intr_hd_handler(uint8_t irq_no)
{
  ASSERT(irq_no == 0x2e || irq_no == 0x2f);
  uint8_t ch_no = irq_no - 0x2e;
  struct ide_channel *channel = &channels[ch_no];
  ASSERT(channel->irq_no == irq_no);
  if (channel->expecting_intr)
  {
    channel->expecting_intr = false;
    sema_up(&channel->disk_done);

    inb(reg_status(channel));
  }
}

/*硬盘 初始化*/
void ide_init()
{
  printk("[ide_init] start\n");

  uint8_t hd_cnt = *((uint8_t *)(0x475)); //1mb下内存1v1映射的
  ASSERT(hd_cnt > 0);
  list_init(&partition_list);
  channel_cnt = DIV_ROUND_UP(hd_cnt,2);
  
  struct ide_channel *channel;
  uint8_t channel_no = 0;
  uint8_t dev_no = 0;

  /*初始化ata通道*/
  while (channel_no < channel_cnt)
  {
    channel = &channels[channel_no];
    sprintf(channel->name,"ide%d",channel_no);

    /*初始化channel的主从通道的寄存器端口和irq*/
    switch(channel_no)
    {
      case 0:
        channel->port_base = 0x1F0;
        channel->irq_no = 0x20 + 14;
        break;
      case 1:
        channel->port_base = 0x170;
        channel->irq_no = 0x20 + 15;
        break;
    }

    channel->expecting_intr = false;
    lock_init(&channel->lock);
    sema_init(&channel->disk_done,0);

    //每个通道添加中断处理程序
    register_handler(channel->irq_no,intr_hd_handler);

    //打印两个硬盘的参数以及分区的信息
    while (dev_no < 2)
    {
      //由通道得到disk
      struct disk *hd = &channel->devices[dev_no];
      //在disk的指向自己所属于的channel
      hd->my_channel = channel;

      hd->dev_no = dev_no;
      //这里channel*2没用,channel_cnt由磁盘数量推出来的
      sprintf(hd->name,"sd%c",'a'+channel_no*2+dev_no);
      identify_disk(hd);
      //kernel所在的hd60M.img这个裸盘就不scan了
      if (dev_no != 0)
      {
        partition_scan(hd,0);
      }
      p_no = l_no = 0;
      dev_no++;
    }
    dev_no = 0;
    channel_no++;
  }

  printk("\n   [All Partition Info]\n");
  list_traversal(&partition_list,partition_info,(int)NULL);
  printk("[ide_init] done\n");
}
