#include <stdio.h>

int main()
{
  FILE *fp = fopen("1.in","r+");
  while (!feof(fp))
  {
    int num;
    fscanf(fp,"%d",&num);
      printf("%d\n",num);
  }
  return 0;
}
