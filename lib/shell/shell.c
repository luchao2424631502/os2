#include "shell.h"
#include "stdio.h"
#include "debug.h"
#include "global.h"
#include "file.h"
#include "fs.h"
#include "string.h"
#include "syscall.h"
#include "assert.h"

#define cmd_len 128     //命令最长长度
#define MAX_ARG_NR  16  //最多支持15个参数

static char cmd_line[cmd_len] = {0};

//shell当前位于的目录路径
char cwd_cache[64] = {0};

void print_prompt()
{
  char *user = "user1";
  char *hostname = "localhost";
  printf("[%s@%s %s]$ ",user,hostname,cwd_cache);
}

//从键盘缓冲区中读取count字节到buf中
static void readline(char *buf,int32_t count)
{
  ASSERT(buf != NULL && count > 0);
  char *pos = buf;
  while (read(stdin_no,pos,1) != -1 && (pos - buf) < count)
  {
    switch(*pos)
    {
      case '\n':
      case '\r':
        *pos = 0;
        putchar('\n');
        return ;
      case '\b':
        if (buf[0] != '\b')
        {
          --pos;
          putchar('\b');//光标吃掉一个字符
        }
        break;
      default:
        putchar(*pos);
        pos++;
    }
  }
  printf("lib/shell/shell.c readline(): can't find enter_key in the cmd_line,out of range 128\n");
}

void my_shell()
{
  cwd_cache[0] = '/';

  while (1)
  {
    print_prompt();
    memset(cmd_line,0,cmd_len);
    readline(cmd_line,cmd_len);
    if (cmd_line[0] == 0)
      continue;
  }
  panic("lib/shell/shell.c my_shell: should not be here\n");
}
