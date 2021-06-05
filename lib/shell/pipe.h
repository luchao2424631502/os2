#ifndef __LIB_SHELL_PIPE_H
#define __LIB_SHELL_PIPE_H

#include "stdint.h"
#include "global.h"

#define PIPE_FLAG 0xFFFF

bool is_pipe(uint32_t);
int32_t sys_pipe(int32_t pipefd[2]);
uint32_t pipe_read(int32_t,void *,uint32_t);
uint32_t pipe_write(int32_t,const void *,uint32_t);
#endif
