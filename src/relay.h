#ifndef RELAY_H
#define RELAY_H

/*
 * 原始字节中继：成功 0，失败 -1。调用方需先把两端设成非阻塞。
 * 内部：工作线程里跑 poll_relay；main 侧 create/join。
 */
int relay_bytes(int in_fd, int out_fd);

#endif
