#ifndef __USERPROG_FORK_H
#define __USERPROG_FORK_H

#include "thread.h"

/*只能由用户进程调用,*/
pid_t sys_fork();

#endif
