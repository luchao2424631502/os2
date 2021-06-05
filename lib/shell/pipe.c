#include "pipe.h"
#include "fs.h"
#include "file.h"
#include "memory.h"
#include "ioqueue.h"

/*判断文件描述符对应的文件是不是管道*/
bool is_pipe(uint32_t local_fd)
{
  uint32_t global_fd = fd_local2global(local_fd);
  return file_table[global_fd].fd_flag == PIPE_FLAG;
}

/*创建管道,成功返回0,失败返回-1*/
int32_t sys_pipe(int32_t pipefd[2])
{
  int32_t global_fd = get_free_slot_in_global();

  //在内核空间申请1页缓冲区作为管道
  file_table[global_fd].fd_inode = get_kernel_pages(1);

  //初始化环形缓冲区
  ioqueue_init((struct ioqueue *)file_table[global_fd].fd_inode);
  if (file_table[global_fd].fd_inode == NULL)
  {
    return -1;
  }

  //标记为pipe文件
  file_table[global_fd].fd_flag = PIPE_FLAG;

  file_table[global_fd].fd_pos = 2;//个数
  pipefd[0] = pcb_fd_install(global_fd);
  pipefd[1] = pcb_fd_install(global_fd);
  return 0;
}

/*从管道中读取数据*/
uint32_t pipe_read(int32_t fd,void *buf,uint32_t count)
{
  char *buffer = buf;
  uint32_t bytes_read = 0;
  uint32_t global_fd = fd_local2global(fd);

  /*获取ring buffer地址*/
  struct ioqueue *ioq = (struct ioqueue *)file_table[global_fd].fd_inode;

  //获取ring buffer长度
  uint32_t ioq_len = ioq_length(ioq);
  //选取下限长度来读取
  uint32_t size = ioq_len > count ? count : ioq_len;
  while (bytes_read < size)
  {
    *buffer = ioq_getchar(ioq);
    bytes_read++;
    buffer++;
  }
  return bytes_read;
}

/*向管道中写入数据*/
uint32_t pipe_write(int32_t fd,const void *buf,uint32_t count)
{
  uint32_t bytes_write = 0;
  uint32_t global_fd = fd_local2global(fd);
  struct ioqueue* ioq = (struct ioqueue *)file_table[global_fd].fd_inode;

  /*选择下限长度来写入*/
  uint32_t ioq_left = BUF_SIZE - ioq_length(ioq);
  uint32_t size = ioq_left > count ? count : ioq_left;

  const char *buffer = buf;
  while (bytes_write < size)
  {
    ioq_putchar(ioq,*buffer);
    bytes_write++;
    buffer++;
  }

  return bytes_write;
}
