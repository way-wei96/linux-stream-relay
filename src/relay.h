#ifndef RELAY_H
#define RELAY_H

/*
 * 原始字节中继：成功 0，失败 -1。
 * 从 in_fd 读、往 out_fd 写；读/写各一线程，经有界 ring 交接。
 * 两端使用阻塞 IO。
 */
int relay_bytes(int in_fd, int out_fd);

#endif
