#ifndef __IO_H__
#define __IO_H__

void io_init();

void io_wait();

void io_need_read(int fd);

void io_need_write(int fd);

int io_wait_read(int fd);

int io_wait_write(int fd);

#endif
