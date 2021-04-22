#include "vramio.h"
#include "string.h"

#define ONE_BYTE  1

/*向vram中写1byte颜色*/
void write_mem8(uint32_t addr,uint8_t data)
{
  memset((void*)addr,data,ONE_BYTE);
}
