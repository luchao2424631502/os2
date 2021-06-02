[bits 32]
extern main
section .text
global _start
_start:
  push ebx    ;argv
  push ecx    ;argc
  call main
