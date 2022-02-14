#ifndef __GRAPHICS_MOUSE_H
#define __GRAPHICS_MOUSE_H  

#include "stdint.h"
#include "sheet.h"

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

/*mouse_sample_rate:鼠标采样率*/
#define SAMPLE_RATE_10    0x0a
#define SAMPLE_RATE_20    0x14
#define SAMPLE_RATE_40    0x28
#define SAMPLE_RATE_80    0x50
#define SAMPLE_RATE_DEFAULT SAMPLE_RATE_80
#define SAMPLE_RATE_100   0x64
#define SAMPLE_RATE_200   0xC8


extern struct FIFO8 mouse_buf;
extern int mx,my;
extern char mcursor[256];

void init_mouse_cursor8(unsigned char *,unsigned char );
void putblock8(uint8_t *,int ,int ,int ,int ,int ,char *,int );

/*注册ps/2鼠标中断*/
void mouse_init();

void k_mouse(struct SHEET *sht_mouse, struct SHEET *sht_win,
             unsigned char *buf_win, struct SHEET *sht_back,
             unsigned char *buf_back);

#endif
