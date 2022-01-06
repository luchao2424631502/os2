#ifndef __GRAPHICS_MOUSE_H
#define __GRAPHICS_MOUSE_H  

#include "stdint.h"

/*鼠标中芯片的端口*/
#define PORT_KEYDAT 0x0060
#define PORT_KEYSTA 0x0064
#define PORT_KEYCMD 0x0064
// keysta_send_notready
#define KEYSTA_SEND_NOTREADY  0x02
#define KEYCMD_WRITE_MODE 0x60
#define KBC_MODE  0x47

/*向鼠标发送数据前,先向端口发送0xd4,
 * 然后任何写入0x60端口的数据都会发送给鼠标*/
#define KEYCMD_SENDTO_MOUSE 0xd4
/*向0x60端口写入0xf4,鼠标被激活,立刻向cpu发出中断*/
#define MOUSECMD_ENABLE 0xf4

extern struct FIFO8 mouse_buf;
extern int mx,my;
extern char mcursor[256];

void init_mouse_cursor8(char *,char );
void putblock8(uint8_t *,int ,int ,int ,int ,int ,char *,int );

/*注册ps/2鼠标中断*/
void mouse_init();

void k_mouse(void* UNUSED);

#endif

