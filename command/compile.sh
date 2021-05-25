#!/bin/bash

#通过判断上层../lib和../tmp是否存在来判断当前是否在command中
if [[ ! -d "../lib" || ! -d "../tmp" ]];then
  echo "dependent dir don't exist!"
  cwd=$(pwd)
  cwd=${cwd##*/}#删掉从左边开始的最后一个/及前面所有内容
  cwd=${cwd%/}  #删掉从右边开始的第一个/
  if [[ $cwd != "command" ]];then
    echo -e "当前工作目录不在command目录下\n"
  fi
  exit
fi

BIN="prog_no_arg"
#gcc -Wall -c -m32 -fno-builtin -fno-stack-protector -W -Wno-implicit-fallthrough
CFLAGS="-m32 -Wall -c -fno-builtin -fno-stack-protector -W -Wstrict-prototypes -Wmissing-prototypes -Wsystem-headers"
LIB="../lib/"
OBJS="../tmp/string.o ../tmp/syscall.o ../tmp/stdio.o ../tmp/assert.o"
DD_IN=$BIN
DD_OUT="/home/luchao/Work/os2/hd60M.img"

#ld -Ttext 0xc0001500 -e main -m elf_i386 
gcc $CFLAGS -I $LIB -o $BIN".o" $BIN".c"
ld -m elf_i386 -e main $BIN".o" $OBJS -o $BIN
SEC_CNT=$(ls -l $BIN|awk '{printf("%d",($5+511)/512)}')

if [[ -f $BIN ]];then
  dd if=./$DD_IN of=$DD_OUT bs=512 count=$SEC_CNT seek=300 conv=notrunc
fi

