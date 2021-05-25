#include "exec.h"
#include "interrupt.h"
#include "memory.h"
#include "fs.h"
#include "string.h"
#include "thread.h"

extern void intr_exit();

typedef uint32_t Elf32_Word,Elf32_Addr,Elf32_Off;
typedef uint16_t Elf32_Half;

/*32位 Elf 头*/
struct Elf32_Ehdr
{
  unsigned char e_ident[16];
  Elf32_Half      e_type;       //elf类型 3种,1.可重定位文件,2.共享对象(动态库 3.exec
  Elf32_Half      e_machine;    //平台
  Elf32_Word      e_version;    //elf版本
  Elf32_Addr      e_entry;      //入口地址
  Elf32_Off       e_phoff;      //program表在文件中的偏移
  Elf32_Off       e_shoff;      //section表在文件中的偏移
  Elf32_Word      e_flags;      //
  Elf32_Half      e_ehsize;     //elf header size,elf头的大小
  Elf32_Half      e_phentsize;  //program header 项大小
  Elf32_Half      e_phnum;      //program header 项个数
  Elf32_Half      e_shentsize;  //section header 项大小
  Elf32_Half      e_shnum;      //section header 项个数
  Elf32_Half      e_shstrndx;   //section header string table 的索引
};

//段由section组成

/*Program header,段描述符头(Segment)*/
struct Elf32_Phdr
{
  Elf32_Word      p_type;
  Elf32_Off       p_offset;
  Elf32_Addr      p_vaddr;
  Elf32_Addr      p_paddr;
  Elf32_Word      p_filesz;
  Elf32_Word      p_memsz;
  Elf32_Word      p_flags;
  Elf32_Word      p_align;
};

/* Segment类型 */
enum segment_type 
{
  PT_NULL,        //
  PT_LOAD,        //可加载程序段
  PT_DYNAMIC,     //动态加载   信息
  PT_INTERP,      //动态加载器 名称
  PT_NOTE,        //注意信息
  PT_SHLIB,       //保留
  PT_PHDR         //程序表头
};

/*将段(fd文件的offset偏移处的filesz大小的段)加载到虚拟地址为vaddr的内存处*/
static bool segment_load(int32_t fd,uint32_t offset,uint32_t filesz,uint32_t vaddr)
{
  uint32_t vaddr_first_page = vaddr & 0xfffff000;//所在的页地址
  uint32_t size_in_first_page = PG_SIZE - (vaddr & 0x00000fff);//段占据第1个页的大小
  uint32_t occupy_pages = 0;

  //需要多个页,计算占用页的个数
  if (filesz > size_in_first_page)
  {
    uint32_t left_size = filesz - size_in_first_page;
    occupy_pages = DIV_ROUND_UP(left_size,PG_SIZE) + 1;
  }
  //1个页足够
  else 
  {
    occupy_pages = 1;
  }

  /*为进程分配内存*/
  uint32_t page_idx = 0;
  uint32_t vaddr_page = vaddr_first_page;//第一个页的地址
  while (page_idx < occupy_pages)
  {
    uint32_t * pde = pde_ptr(vaddr_page);
    uint32_t * pte = pte_ptr(vaddr_page);

    /*先判断页目录(pe)是否存在,然后在判断页表(pt)*/
    if (!(*pde & 0x00000001) || !(*pte & 0x00000001))
    {
      /*不存在则分配内存*/
      if (get_a_page(PF_USER,vaddr_page) == NULL)
      {
        return false;
      }
    }

    vaddr_page += PG_SIZE;//用户进程的虚拟地址空间是连续的
    page_idx++;
  }

  sys_lseek(fd,offset,SEEK_SET);
  sys_read(fd,(void*)vaddr,filesz);
  return true;
}

/*加载路径为pathname的用户程序,失败返回-1,成功返回程序的入口地址*/
static int32_t load(const char *pathname)
{
  int32_t ret = -1;
  struct Elf32_Ehdr elf_header; //elf文件头
  struct Elf32_Phdr prog_header;//Segment头
  memset(&elf_header,0,sizeof(struct Elf32_Ehdr));

  int32_t fd = sys_open(pathname,O_RDONLY);
  if (fd == -1)
  {
    return -1;
  }

  /*读取elf头*/
  if (sys_read(fd,&elf_header,sizeof(struct Elf32_Ehdr)) != sizeof(struct Elf32_Ehdr))
  {
    ret = -1;
    goto done;
  }

  /*检验elf头*/
  if (memcmp(elf_header.e_ident,"\177ELF\1\1\1",7)
      || elf_header.e_type != 2
      || elf_header.e_machine != 3
      || elf_header.e_version != 1 
      || elf_header.e_phnum > 1024
      || elf_header.e_phentsize != sizeof(struct Elf32_Phdr))
  {
    ret = -1;
    goto done;
  }

  //Segment header
  Elf32_Off  prog_header_offset = elf_header.e_phoff;
  Elf32_Half prog_header_size   = elf_header.e_phentsize; 

  uint32_t prog_idx = 0;
  /*遍历所有的segment*/
  while (prog_idx < elf_header.e_phnum)
  {
    memset(&prog_header,0,prog_header_size);

    sys_lseek(fd,prog_header_offset,SEEK_SET);

    //只获取程序头
    if (sys_read(fd,&prog_header,prog_header_size) != prog_header_size)
    {
      ret = -1;
      goto done;
    }

    /*是可加载段,就加载到内存中*/
    if (PT_LOAD == prog_header.p_type)
    {
      if (!segment_load(fd,prog_header.p_offset,prog_header.p_filesz,prog_header.p_vaddr))
      {
        ret = -1;
        goto done;
      }
    }

    prog_header_offset += elf_header.e_phentsize;
    prog_idx++;
  }

  ret = elf_header.e_entry;//返回程序入口的虚拟地址
done:
  sys_close(fd);
  return ret;
}

/*用path处的程序替换当前进程*/
int32_t sys_execv(const char *path,const char *argv[])
{
  uint32_t argc = 0;
  while (argv[argc])
  {
    argc++;
  }

  int32_t entry_point = load(path);
  if (entry_point == -1)
  {
    return -1;
  }

  struct task_struct *cur = running_thread(); 
  //修改进程名
  memcpy(cur->name,path,TASK_NAME_LEN);
  cur->name[TASK_NAME_LEN-1] = 0;

  /* 为什么昨天我在这一段代码产生疑问?:
   * 1.intr_stack中的self_kstack在intr_stack最低处,但是指向的是仍是pcb中的数据,因为栈都放在Pcb的最高端,往下开始排
   * */
  struct intr_stack *intr_0_stack = (struct intr_stack*)((uint32_t)cur + PG_SIZE - sizeof(struct intr_stack));

  intr_0_stack->ebx = (int32_t)argv;
  intr_0_stack->ecx = argc;
  intr_0_stack->eip = (void*)entry_point;
  //用户栈指针赋值为 用户空间的最高地址
  intr_0_stack->esp = (void*)0xc0000000;    

  asm volatile ("movl %0,%%esp; jmp intr_exit"::"g"(intr_0_stack):"memory");

  return 0;
}
