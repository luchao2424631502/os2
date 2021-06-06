#include "buildin_cmd.h"
#include "assert.h"
#include "dir.h"
#include "fs.h"
#include "syscall.h"
#include "string.h"
#include "global.h"
#include "stdio.h"
#include "shell.h"

//去掉绝对路径中的.和..
static void wash_path(char *old_abs_path,char *new_abs_path)
{
  assert(old_abs_path[0] == '/');
  char name[MAX_FILE_NAME_LEN] = {0};
  char *sub_path = old_abs_path;

  //拿到路径最里面的路径名
  sub_path = path_parse(sub_path,name);
  if (name[0] == 0)//sub_path根处的名称就是空(只有/
  {
    new_abs_path[0] = '/';
    new_abs_path[1] = 0;
    return ;
  }

  new_abs_path[0] = 0;
  strcat(new_abs_path,"/");

  while (name[0])
  {
    //这一个路径名称是 .. (上一级目录)
    if (!strcmp("..",name))
    {
      //new_abs_path不是根目录/,就去掉/
      char *slash_ptr = strrchr(new_abs_path,'/');
      //不等于就说明new_abs_path不是根目录,由/a/b -> /a
      if (slash_ptr != new_abs_path)
      {
        *slash_ptr = 0;
      }
      //new_abs_path已经是根目录了,不能在向上一级目录走了,由/a->/
      else 
      {
        *(slash_ptr + 1) = 0;
      }
    }
    //名称不是 .,即是目录名或文件名
    else if (strcmp(".",name))
    {
      //不是根目录(/),添加/
      if (strcmp(new_abs_path,"/"))
      {
        strcat(new_abs_path,"/");
      }
      strcat(new_abs_path,name);//添加文件or目录名
    }
    //当前目录不需要处理,".",跳过继续处理下一个就行

    memset(name,0,MAX_FILE_NAME_LEN);
    if (sub_path)//子路径不是空,继续下去,否则name[0]=0退出
    {
      sub_path = path_parse(sub_path,name);
    }
  }
}

/*将路径中的..和.去掉*/
void make_clear_abs_path(char *path,char *final_path)
{
  char abs_path[MAX_PATH_LEN] = {0};
  //给出的路径是相对路径,转化为绝对路径
  if (path[0] != '/')
  {
    memset(abs_path,0,MAX_PATH_LEN);
    if (getcwd(abs_path,MAX_PATH_LEN) != NULL)
    {
      if (!((abs_path[0] == '/') && (abs_path[1] == 0)))//当前目录不是根目录
      {
        strcat(abs_path,"/");
      }
    }
  }
  strcat(abs_path,path);
  wash_path(abs_path,final_path);
}

/*----------添加一系列的shell,内建命令----------------------*/
void buildin_pwd(uint32_t argc,char **argv UNUSED)
{
  if (argc != 1)
  {
    printf("shell/buildin_cmd.c buildin_pwd(): pwd: no argument support!\n");
    return ;
  }
  else 
  {
    //得到当前的路径
    if (NULL != getcwd(final_path,MAX_PATH_LEN))
    {
      printf("%s\n",final_path);
    }
    //失败
    else 
    {
      printf("pwd: get current work directory failed!\n");
    }
  }
}

/*cd命令*/
char *buildin_cd(uint32_t argc,char **argv)
{
  if (argc > 2)
  {
    printf("shell/buildin_cmd.c buildin_cd(): cd: only support 1 argument\n");
    return NULL;
  }

  //将argv[1] wash成绝对路径
  if (argc == 1)
  {
    final_path[0] = '/';
    final_path[1] = 0;
  }
  else 
  {
    make_clear_abs_path(argv[1],final_path);
  }

  //进入final_path目录
  if (chdir(final_path) == -1)
  {
    printf("shell/buildin_cmd.c buildin_cd(): cd: no such directory %s\n",final_path);
    return NULL;
  }
  return final_path;
}

/*ls命令*/
void buildin_ls(uint32_t argc,char **argv)
{
  char *pathname = NULL;

  struct stat file_stat;
  memset(&file_stat,0,sizeof(struct stat));

  bool long_info = false;
  uint32_t arg_path_nr = 0;
  uint32_t arg_idx = 1;//argv[0]是"ls",只需要扫描参数

  //解析参数和路径
  while (arg_idx < argc)
  {
    //参数选项
    if (argv[arg_idx][0] == '-')
    {
      if (!strcmp("-l",argv[arg_idx]))
      {
        long_info = true;
      }
      else if (!strcmp("--help",argv[arg_idx]))
      {
        printf("usage: -l list all infomation about the file.\n--help for help\nlist all files in the current dirctory if no option\n"); 
        return ;
      }
      //暂不支持其它选项
      else 
      {
        printf("ls: invalid option %s\nTry `ls --help' for more information.\n", argv[arg_idx]);
        return ;
      }
    }
    //路径
    else 
    {
      if (arg_path_nr == 0)
      {
        pathname = argv[arg_idx];
        arg_path_nr = 1;
      }
      else 
      {
        printf("ls: only support one path\n");
        return ;
      }
    }
    arg_idx++;
  }

  //默认显示cwd路径下的文件,所以pathname=getcwd()
  if (pathname == NULL)
  {
    if (NULL != getcwd(final_path,MAX_PATH_LEN))
    {
      pathname = final_path;
    }
    else 
    {
      printf("ls: getcwd for default path failed\n");
      return ;
    }
  }
  //把用户给出的路径清洗以下
  else 
  {
    make_clear_abs_path(pathname,final_path);
    pathname = final_path;
  }

  /*路径已经处理完,拿到文件信息*/
  if (stat(pathname,&file_stat) == -1)
  {
    printf("ls: cannot access %s: No such file or directory\n",pathname);
    return ; 
  }

  //目录文件
  if (file_stat.st_filetype == FT_DIRECTORY)
  {
    struct dir *dir = opendir(pathname);
    struct dir_entry *dir_e = NULL;
    char sub_pathname[MAX_PATH_LEN] = {0};

    uint32_t pathname_len = strlen(pathname);
    uint32_t last_char_idx = pathname_len - 1;

    memcpy(sub_pathname,pathname,pathname_len);
    if (sub_pathname[last_char_idx] != '/')
    {
      sub_pathname[pathname_len] = '/';
      pathname_len++;
    }
    //目录文件的指针指向开头
    rewinddir(dir);
    //详细打印目录项
    if (long_info)
    {
      char ftype;
      printf("total: %d\n",file_stat.st_size);
      while ((dir_e = readdir(dir)))
      {
        ftype = 'd';
        if (dir_e->f_type == FT_REGULAR)
        {
          ftype = '-';
        }
        sub_pathname[pathname_len] = 0;
        strcat(sub_pathname,dir_e->filename);
        memset(&file_stat,0,sizeof(struct stat));
        //获取文件状态失败
        if (stat(sub_pathname,&file_stat) == -1)
        {
          printf("ls: cannot access %s: No such file or directory\n",dir_e->filename);
          return ;
        }
        printf("%c  %d  %d  %s\n", ftype, dir_e->i_no, file_stat.st_size, dir_e->filename);
      }
    }
    else 
    {
      //遍历目录文件的数据(所有的目录项)
      while ((dir_e = readdir(dir)))
      {
        printf("%s ",dir_e->filename);
      }
      printf("\n");
    }
    closedir(dir);
  }
  //普通文件
  else 
  {
    //详细输出
    if (long_info)
    {
      printf("-  %d  %d  %s\n",file_stat.st_ino,file_stat.st_size,pathname);
    }
    else 
    {
      printf("%s\n",pathname);
    }
  }
}

/**/
void buildin_ps(uint32_t argc,char **argv UNUSED)
{
  if (argc != 1)
  {
    printf("ps: no argument support\n");
    return ;
  }
  ps();
}

/**/
void buildin_clear(uint32_t argc,char **argv UNUSED)
{
  if (argc != 1)
  {
    printf("clear: no argument support!\n");
    return ;
  }
  clear();
}

/**/
int32_t buildin_mkdir(uint32_t argc,char **argv)
{
  int32_t ret = -1;
  if (argc != 2)
  {
    printf("mkdir: only support 1 argument\n");
  }
  else 
  {
    make_clear_abs_path(argv[1],final_path);
    //创建的不是根目录
    if (strcmp("/",final_path))
    {
      if (mkdir(final_path) == 0)
      {
        ret = 0;
      }
      else
      {
        printf("mkdir: create directory %s failed\n",argv[1]);
      }
    }
  }
  return ret;
}

/**/
int32_t buildin_rmdir(uint32_t argc,char **argv)
{
  int32_t ret = -1;
  if (argc != 2)
  {
    printf("rmdir: only support 1 argument!\n");
  }
  else 
  {
    //清理用户给出来的路径
    make_clear_abs_path(argv[1],final_path);
    //只要不是根目录,根目录肯定是不能够删除的
    if (strcmp("/",final_path))
    {
      if (rmdir(final_path) == 0)
      {
        ret = 0;
      }
      //rmdir()执行失败
      else 
      {
        printf("rmdir: remove %s failed.\n",argv[1]);
      }
    }
  }
  return ret;
}

/**/
int32_t buildin_rm(uint32_t argc,char **argv)
{
  int32_t ret = -1;
  if (argc != 2)
  {
    printf("rm: only support 1 argument\n");
  }
  else 
  {
    make_clear_abs_path(argv[1],final_path);
    if (strcmp("/",final_path))
    {
      if (unlink(final_path) == 0)
      {
        //删除文件成功
        ret = 0;
      }
      else 
      {
        printf("rm: delete %s failed\n",argv[1]);
      }
    }
  }
  return ret;
}

void buildin_help(uint32_t argc UNUSED,char **argv UNUSED)
{
  help();
}
