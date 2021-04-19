#include "bitmap.h"
#include "stdint.h"
#include "string.h"
#include "print.h"
#include "interrupt.h"
#include "debug.h"

/*初始化bitmap*/
void bitmap_init(struct bitmap* btmp)
{
  memset(btmp->bits,0,btmp->btmp_bytes_len);
}

/*测试指定位的bit是否为1*/
bool bitmap_scan_test(struct bitmap *btmp,uint32_t bit_index)
{
  uint32_t byte_index = bit_index/8;
  uint32_t bit_off = bit_index%8;
  return (btmp->bits[byte_index] & (BITMAP_MASK << bit_off));
}

/*在bitmap中申请连续cnt个bit,成功找到则返回下标*/
int bitmap_scan(struct bitmap *btmp,uint32_t cnt)
{
  uint32_t index_byte = 0;
  while((0xff == btmp->bits[index_byte]) && (index_byte < btmp->btmp_bytes_len))
  {
    index_byte++;
  }

  ASSERT(index_byte < btmp->btmp_bytes_len); //如果发生则会在这里停止
  if (index_byte == btmp->btmp_bytes_len)
  {
    return -1;
  }
  //找到了空闲的字节
  int index_bit = 0;
  while((uint8_t)(BITMAP_MASK << index_bit) & btmp->bits[index_byte])
  {
    index_bit++;
  }

  int bit_index_start = index_byte * 8 + index_bit; //总的bit位数的下标
  if (cnt == 1)
  {
    return bit_index_start;
  }

  uint32_t bit_left = (btmp->btmp_bytes_len * 8 - bit_index_start);
  uint32_t next_bit = bit_index_start + 1;
  uint32_t count = 1;
  
  bit_index_start = -1;
  while(bit_left-- > 0)
  {
    if (!(bitmap_scan_test(btmp,next_bit)))
    {
      count++;
    }
    else //可能是第2轮scan,导致后面有的bit在使用,只能跳过继续scan下去.
    {
      count = 0;
    }
    if (count == cnt)
    {
      bit_index_start = next_bit - cnt + 1;
      break;
    }
    next_bit++;
  }
  return bit_index_start;
}

/*将bitmap的bit_index处的bit设置为value*/
void bitmap_set(struct bitmap *btmp,uint32_t bit_index,int8_t value)
{
  ASSERT((value == 0) || (value == 1));
  uint32_t byte_index = bit_index / 8;
  uint32_t bit_off = bit_index % 8;

  if (value)
  {
    btmp->bits[byte_index] |= (BITMAP_MASK << bit_off);
  }
  else
  {
    btmp->bits[byte_index] &= ~(BITMAP_MASK << bit_off);
  }
}
