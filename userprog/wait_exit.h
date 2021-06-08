#ifndef __USERPROG_WAIT_EXIT_H
#define __USERPROG_WAIT_EXIT_H

#include "thread.h"

pid_t sys_wait(int32_t *);
void sys_exit(int32_t);

#endif
