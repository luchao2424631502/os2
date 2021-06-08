#include "stdio.h"
#include "syscall.h"
#include "string.h"

int main(int argc,char **argv)
{
  int arg_idx = 0;
  while (arg_idx < argc)
  {
    printf("argv[%d] is %s\n",arg_idx,argv[arg_idx]);
    arg_idx++;
  }
  int pid = fork();
  if (pid)//父进程
  {
    int delay = 900000;
    while (delay--);
    printf("\n    I'm father prog,pid is %d\n",getpid());
    ps();
  }
  else//子进程
  {
    char abs_path[512] = {0};
    printf("\n    I'm child prog,pid is %d, exec %s right now\n,",getpid(),argv[1]);
    if (argv[1][0] != '/')
    {
      getcwd(abs_path,512);
      strcat(abs_path,"/");
      strcat(abs_path,argv[1]);
      execv(abs_path,argv);
    }
    else 
    {
      execv(argv[1],argv);
    }
  }
  while (1);
  return 0;
}
