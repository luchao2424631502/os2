#include "list.h"
#include "interrupt.h"

/*1.初始化链表*/
void list_init(struct list *list)
{
  list->head.prev = NULL;
  list->head.next = &list->tail;
  list->tail.prev = &list->head;
  list->tail.next = NULL;
}

/*2.在元素before前插入元素elem*/
void list_insert_before(struct list_elem *before,struct list_elem *elem)
{
  //不知道当前是否开启中断,但是要关闭时钟中断,退出后恢复原来时钟中断的状态
  enum intr_status intr_old_status = intr_disable();

  elem->prev = before->prev;
  elem->next = before;
  before->prev->next = elem;
  before->prev = elem;

  intr_set_status(intr_old_status);
}

/*3.添加元素到队列的队首*/
void list_push(struct list *list,struct list_elem *elem)
{
  //原来作者的链表意思是不希望变动list.head和list.tail两个变量的值,所以只能插在head的后一个的前面
  list_insert_before(list->head.next,elem);
}

/*4.添加元素到链表的末尾*/
void list_append(struct list *list,struct list_elem *elem)
{
  list_insert_before(&list->tail,elem);
}

/*5.将当前元素从链表中移除*/
void list_remove(struct list_elem *elem)
{
  enum intr_status intr_old_status = intr_disable();

  elem->prev->next = elem->next;
  elem->next->prev = elem->prev;

  intr_set_status(intr_old_status);
}

/*6.将链表的第一个元素弹出去*/
struct list_elem *list_pop(struct list *list)
{
  struct list_elem *elem = list->head.next;
  list_remove(elem);
  return elem;
}

/*7.判断链表中是否存在元素 obj_elem*/
bool elem_find(struct list *list,struct list_elem *obj_elem)
{
  struct list_elem *elem = list->head.next;
  while (elem != &list->tail)
  {
    if (elem == obj_elem)
    {
      return true;
    }
    elem = elem->next;
  }
  return false;
}

/*8.满足函数条件,返回元素指针*/
struct list_elem *list_traversal(struct list *list,function func,int arg)
{
  struct list_elem *elem = list->head.next;
  if (list_empty(list))
  {
    return NULL;
  }

  while(elem != &list->tail)
  {
    if (func(elem,arg))
    {
      return elem;
    }
    elem = elem->next;
  }
  return NULL;
}

/*9.返回链表长度*/
uint32_t list_len(struct list *list)
{
  struct list_elem *elem = list->head.next;
  uint32_t len = 0;
  while(elem != &list->tail)
  {
    len++;
    elem = elem->next;
  }
  return len;
}

/*10.判断链表是否为空*/
bool list_empty(struct list *list)
{
  return list->head.next == &list->tail ? true : false;
}
