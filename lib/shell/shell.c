#include "shell.h"
#include "stdio.h"
#include "debug.h"
#include "global.h"
#include "file.h"
#include "fs.h"
#include "string.h"
#include "syscall.h"
#include "assert.h"
#include "buildin_cmd.h"

#define MAX_ARG_NR  16  //最多支持15个参数

static char cmd_line[MAX_PATH_LEN] = {0};
/*洗路径时的缓冲*/
char final_path[MAX_PATH_LEN] = {0};

//shell当前位于的目录路径
char cwd_cache[MAX_PATH_LEN] = {0};

void print_prompt()
{
  char *user = "user1";
  char *hostname = "localhost";
  printf("[%s@%s %s]$ ",user,hostname,cwd_cache);
}

//从键盘缓冲区中读取count字节到buf中
static void readline(char *buf,int32_t count)
{
  // ASSERT(buf != NULL && count > 0);
  assert(buf != NULL && count > 0);
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
        if (cmd_line[0] != '\b')
        {
          --pos;
          putchar('\b');//光标吃掉一个字符
        }
        break;
      case 'l' - 'a'://快捷键:clear+保留输入的命令
        *pos = 0;
        clear();
        print_prompt();
        printf("%s",buf);
        break;

      case 'u' - 'a':
        while (buf != pos)
        {
          putchar('\b');
          *(pos--) = 0;
        }
        break;

      default:
        putchar(*pos);
        pos++;
    }
  }
  printf("lib/shell/shell.c readline(): can't find enter_key in the cmd_line,out of range 128\n");
}

/*分析cmd_str,将命令单词根据token分割放入argv,*/
static int32_t cmd_parse(char *cmd_str,char **argv,char token)
{
  assert(cmd_str != NULL);
  int32_t arg_idx = 0;
  //数组初始化
  while (arg_idx < MAX_ARG_NR)
  {
    argv[arg_idx] = NULL;
    arg_idx++;
  }

  char *next = cmd_str;
  int32_t argc = 0;
  while (*next)
  {
    while (*next == token)//可能有多个token
      next++;
    if (*next == 0)//结束
    {
      break;
    }
    argv[argc] = next;

    while (*next && *next != token)//跳过命令单词,找到token
    {
      next++;
    }

    if (*next)//将token变成0
    {
      *next++ = 0;
    }

    if (argc > MAX_ARG_NR)//不能超过范围
    {
      return -1;
    }
    argc++;
  }
  return argc;
}

char *argv[MAX_ARG_NR];
int32_t argc = -1;

void my_shell()
{
  cwd_cache[0] = '/';
  while (1)
  {
    print_prompt();
    memset(final_path,0,MAX_PATH_LEN);
    memset(cmd_line,0,MAX_PATH_LEN);
    readline(cmd_line,MAX_PATH_LEN);
    if (cmd_line[0] == 0)
      continue;
    argc = -1;
    argc = cmd_parse(cmd_line,argv,' ');
    if (argc == -1)
    {
      printf("num of arguments exceed %d\n",MAX_ARG_NR);
      continue;
    }

    if (!strcmp("ls",argv[0]))
    {
      buildin_ls(argc,argv);
    }
    else if (!strcmp("cd",argv[0]))
    {
      //cd后,要更新用户当前的目录路径
      if (buildin_cd(argc,argv) != NULL)
      {
        memset(cwd_cache,0,MAX_PATH_LEN);
        strcpy(cwd_cache,final_path);
      }
    }
    else if (!strcmp("pwd",argv[0]))
    {
      buildin_pwd(argc,argv);
    }
    else if (!strcmp("ps",argv[0]))
    {
      buildin_ps(argc,argv);
    }
    else if (!strcmp("clear",argv[0]))
    {
      buildin_clear(argc,argv);
    }
    else if (!strcmp("mkdir",argv[0]))
    {
      buildin_mkdir(argc,argv);
    }
    else if (!strcmp("rmdir",argv[0]))
    {
      buildin_rmdir(argc,argv);
    }
    else if (!strcmp("rm",argv[0]))
    {
      buildin_rm(argc,argv);
    }
    else 
    {
      printf("external command\n");
    }
  }
  panic("lib/shell/shell.c my_shell: should not be here\n");
}
