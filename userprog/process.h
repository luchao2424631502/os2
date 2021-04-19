#ifndef __USERPROG_PROCESS_H
#define __USERPROG_PROCESS_H

#include "thread.h"
#include "stdint.h"

#define DEFAULT_PRIO      31
#define default_prio      31
#define USER_STACK3_VADDR (0xc0000000 - 0x1000)
#define USER_VADDR_START 0x8048000

void process_execute(void *,char *);
void start_process(void *);
void process_activate(struct task_struct *);
void page_dir_activate(struct task_struct *);
uint32_t *create_page_dir();
void create_user_vaddr_bitmap(struct task_struct *);

#endif
