#include "vramio.h"
#include "string.h"

#define ONE_BYTE  1

/*向vram中写1byte颜色*/
// void write_mem8(uint32_t addr,uint8_t data)
// {
  // memset((void*)addr,data,ONE_BYTE);
// }

//fifo初始化函数
void fifo8_init(struct FIFO8 *fifo)
{
  fifo->size = FIFO8_BUF_SIZE;
  // fifo->buf  = buf;
  fifo->free = FIFO8_BUF_SIZE;  //剩余可用空间
  fifo->flags = 0;
  fifo->p = 0;  //p是下一数据写入的位置
  fifo->q = 0;  //q是下一个数据读出的位置
  return ;
}

//写入1字节
int fifo8_put(struct FIFO8 *fifo,unsigned char data)
{
  if (fifo->free == 0)
  {
    fifo->flags |= FLAGS_OVERRUN;
    return -1;
  }
  fifo->buf[fifo->p] = data;
  fifo->p++;
  //满了
  if (fifo->p == fifo->size)
  {
    fifo->p = 0;
  }
  //剩下的可用空间--
  fifo->free--;
  return 0;
}

//读出1字节
int fifo8_get(struct FIFO8 *fifo)
{
  int data;
  //缓冲区为空
  if (fifo->free == fifo->size)
  {
    return -1;
  }
  data = fifo->buf[fifo->q];
  fifo->q++;
  if (fifo->q == fifo->size)
  {
    fifo->q = 0;
  }
  fifo->free++; //剩下可用的空间++
  return data;
}

//返回当前fifo8里面含有的元素
int fifo8_status(struct FIFO8 *fifo)
{
  return fifo->size - fifo->free;
}

//判断fifo是否为空
int fifo8_empty(struct FIFO8 *fifo)
{
  return (fifo->free == FIFO8_BUF_SIZE)?1:0;
}

//判断fifo8是否满
int fifo8_full(struct FIFO8 *fifo)
{
  return (fifo->free == 0)?1:0;
}
