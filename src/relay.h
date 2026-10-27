#ifndef RELAY_H
#define RELAY_H

/*
 * 原始字节中继：直到输入 EOF 且缓冲写完。成功 0，失败 -1。
 * 调用方需先把两端设成非阻塞。
 */
int relay_bytes(int in_fd, int out_fd);

#endif
