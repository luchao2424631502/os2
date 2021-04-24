#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

int main()
{
  unsigned char font[256][16];

  FILE *fp = fopen("hankaku.txt","r+");
  FILE *out = fopen("3.in","w+");

  int off[8] = {0x80,0x40,0x20,0x10,0x8,0x4,0x2,0x1};

  char ch;
  unsigned char data = 0;
  int index = 0;
  int num = 0;
  int count = 0;
  bool flag = 0;
  while (!feof(fp))
  {
    fread(&ch,1,1,fp);
    if (!(ch == '*' || ch == '.'))
      continue;
     // printf("|%d ",index);
    // printf("%c",ch);
    if (ch == '*')
      flag = 1;
    else 
      flag = 0;
    if (flag)
      data = data | off[count];
    count++;
    if (count == 8)
    {
      count = 0;
      font[index][num] = data;
      num++;
      if (num == 16)
      {
        num = 0;
        index++;
      }
      data = 0;
    }
  }

  for (int i=0; i<256; i++)
  {
    for (int j=0; j<16; j++)
    {
      //生成16进制对应的bitmap
      unsigned char ch = font[i][j];
      fwrite(&ch,1,1,out);

       // char buf[5];
       // sprintf(buf,"%d ",font[i][j]);
       // fwrite(buf,strlen(buf),1,out);
    }
  }
  return 0;
}
