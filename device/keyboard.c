#include "keyboard.h"
#include "print.h"
#include "interrupt.h"
#include "io.h"
#include "global.h"
#include "ioqueue.h"

#include "graphics.h"
#include "color256.h"
#include "font.h"
#include "mouse.h"

#define KBD_BUF_PORT 0x60

/*ascii 可控制字符*/
#define esc '\x1b'
#define backspace '\b'
#define tab '\t'
#define enter '\r'
#define delete '\x7f'

#define char_invisible 0 //以上控制字符属于不可见字符
#define ctrl_l_char     char_invisible
#define ctrl_r_char     char_invisible
#define shift_l_char    char_invisible
#define shift_r_char    char_invisible
#define alt_l_char      char_invisible
#define alt_r_char      char_invisible
#define caps_lock_char  char_invisible

#define shift_l_make  0x2a
#define shift_r_make  0x36
#define alt_l_make    0x38
#define alt_r_make    0xe038
#define alt_r_break   0xe0b8
#define ctrl_l_make   0x1d
#define ctrl_r_make   0xe01d
#define ctrl_r_break  0xe09d
#define caps_lock_make 0x3a

/*键盘缓冲区*/
struct ioqueue kbd_buf;

/*make_code通码索引的数组*/
static char keymap[][2] = {
/*索引(通码) 原本   与shift组合*/
/*0x00*/      {0,0},
/*0x01*/      {esc,esc},
              {'1','!'},
              {'2','@'},
              {'3','#'},
              {'4','$'},
              {'5','%'},
              {'6','^'},
              {'7','&'},
              {'8','*'},
              {'9','('},
              {'0',')'},
/*0x0c*/      {'-','_'},
              {'=','+'},
              {backspace,backspace},
              {tab,tab},
/*0x10*/      {'q','Q'},
              {'w','W'},
              {'e','E'},
              {'r','R'},
              {'t','T'},
              {'y','Y'},
              {'u','U'},
              {'i','I'},
              {'o','O'},
              {'p','P'},
              {'[','{'},
              {']','}'},
/*0x1c*/      {enter,enter},
              {ctrl_l_char,ctrl_l_char},
              {'a','A'},
              {'s','S'},
              {'d','D'},
              {'f','F'},
              {'g','G'},
              {'h','H'},
              {'j','J'},
              {'k','K'},
              {'l','L'},
              {';',':'},
              {'\'','"'},
              {'`','~'},
/*0x2A*/      {shift_l_char,shift_l_char},
              {'\\','|'},
              {'z','Z'},
              {'x','X'},
              {'c','C'},
              {'v','V'},
              {'b','B'},
              {'n','N'},
              {'m','M'},
              {',','<'},
              {'.','>'},
              {'/','?'},
/*0x36*/      {shift_r_char,shift_r_char},
              {'*','*'},
              {alt_l_char,alt_l_char},
              {' ',' '},
              {caps_lock_char,caps_lock_char}
};

static bool ctrl_status,shift_status,alt_status,caps_lock_status;
/*extended 扩展扫描码*/
static bool ext_scancode;

/*键盘中断处理程序*/
static void intr_keyboard_handler()
{

  bool ctrl_down_last = ctrl_status;
  bool shift_down_last = shift_status;
  bool caps_lock_last = caps_lock_status;

  bool break_code;
  /*make code只有8byte,但是要对上一次做操作,所以声明为2byte*/
  uint16_t scancode = inb(KBD_BUF_PORT);

  if (scancode == 0xe0)
  {
    ext_scancode = true;
    return ;
  }

  //上次是ext_scancode,则将scancode合并
  if (ext_scancode)
  {
    scancode = ((0xe000) | scancode);
    ext_scancode = false;
  }

  //断码的第8bit是1,
  break_code = ((scancode & 0x80) != 0);

  if (break_code)
  {
    uint16_t make_code = (scancode &= 0xff7f);

    if (make_code == ctrl_l_make || make_code == ctrl_r_make)
    {
      ctrl_status = false;
    }
    else if (make_code == shift_l_make || make_code == shift_r_make)
    {
      shift_status = false;
    }
    else if (make_code == alt_l_make || make_code == alt_r_make)
    {
      alt_status = false;
    }

    //caps 需要再按一下才能关闭

    return ;
  }
  //scancode是通码,
  else if ((scancode > 0x00 && scancode < 0x3b)||
          (scancode == alt_r_make) || 
          (scancode == ctrl_r_make))
  {
    bool shift = false;
    /*一个键有2个字符的意思(shift+)*/
    if ((scancode < 0x0e) || (scancode == 0x29) ||
        (scancode == 0x1a) || (scancode == 0x1b) ||
        (scancode == 0x2b) || (scancode == 0x27) ||
        (scancode == 0x28) || (scancode == 0x33) ||
        (scancode == 0x34) || (scancode == 0x35))
    {
      if (shift_down_last)
      {
        shift = true;
      }
    }
    /*默认是字母键*/
    else 
    {
      /*shift和caps同时按下 = 小写*/
      if (shift_down_last && caps_lock_last)
      {
        shift = false;
      }
      /*shift和caps任意键被按下 = 大写*/
      else if (shift_down_last || caps_lock_last)
      {
        shift = true;
      }
      else 
      {
        shift = false;
      }
    }
    uint8_t index = (scancode &= 0x00ff);
    char cur_char = keymap[index][shift];

    /*ascii != 0*/
    if (cur_char)
    {
      /*keyboard 第二次修改: 支持ctrl+l / ctrl+u*/
      if ((ctrl_down_last && cur_char == 'l') || (ctrl_down_last && cur_char == 'u'))
      {
        cur_char -= 'a';
      }

      //键盘ioqueue没有满
      if (!ioq_full(&kbd_buf))
      {
        //测试ioqueue 此处put_char打印,有shell处理了,所以不需要put_char来测试了
        // put_char(cur_char);
        ioq_putchar(&kbd_buf,cur_char);

        //在graphics下测试键盘中断是否发生
        // struct BOOT_INFO *bootinfo = (struct BOOT_INFO*)(0xc0000ff0);
        // boxfill8(bootinfo->vram,bootinfo->scrnx,COL8_000000,0,0,16-1,15);
        // putfont8(bootinfo->vram,bootinfo->scrnx,0,0,COL8_ffffff,cur_char);
      }
      return ;
    }

    /*判断本次是否还是控制键,供下一次键盘中断使用*/
    if (scancode == ctrl_l_make || scancode == ctrl_r_make)
    {
      ctrl_status = true;
    }
    else if (scancode == shift_l_make || scancode == shift_r_make)
    {
      shift_status = true;
    }
    else if (scancode == alt_l_make || scancode == alt_r_make)
    {
      alt_status = true;
    }
    /*大小写开关取 上一次的相反状态 */
    else if (scancode == caps_lock_make)
    {
      caps_lock_status = !caps_lock_status;
    }
  }
  /*键盘的其它的Key*/
  else 
  {
    put_str("unknown key\n");
  }
}

/*键盘初始化*/
void keyboard_init()
{
  put_str("[keyboard] init\n");

  /*21/4/9: 键盘ring buffer初始化*/
  ioqueue_init(&kbd_buf);
  register_handler(0x21,intr_keyboard_handler);

  put_str("[keyboard] done\n");
}
