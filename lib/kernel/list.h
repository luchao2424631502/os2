#ifndef __LIB_KERNEL_LIST_H
#define __LIB_KERNEL_LIST_H

#include "global.h"

/*得到struct成员在struct中的偏移地址*/
#define offset(struct_type,member) (int)(&((struct_type*)0)->member)
/*elem_ptr是结构体某个成员的地址*/
#define elem2entry(struct_type,struct_member_name,elem_ptr) \
  (struct_type*)((int)elem_ptr - offset(struct_type,struct_member_name))

/*链表节点: 2个指针变量*/
struct list_elem
{
  struct list_elem *prev;
  struct list_elem *next;
};

/*链表: 2个list_elem变量*/
struct list
{
  struct list_elem head;
  struct list_elem tail;
};

typedef bool (function)(struct list_elem *,int);

void list_init(struct list*);
void list_insert_before(struct list_elem *,struct list_elem *);
void list_push(struct list *,struct list_elem *);
void list_iterate(struct list *);
void list_append(struct list *,struct list_elem *);
void list_remove(struct list_elem *);
struct list_elem *list_pop(struct list *);
bool list_empty(struct list *);
uint32_t list_len(struct list *);
struct list_elem *list_traversal(struct list *,function,int);
bool elem_find(struct list *,struct list_elem *);

#endif
