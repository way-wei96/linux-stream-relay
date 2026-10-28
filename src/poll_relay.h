#ifndef POLL_RELAY_H
#define POLL_RELAY_H

/*
 * 非阻塞 poll 中继：直到输入 EOF 且缓冲写完。成功 0，失败 -1。
 * each_loop 非 NULL 时，每轮循环开头调用一次（hook_arg 原样传入）。
 */
int poll_relay(int in_fd, int out_fd,
               void (*each_loop)(void *hook_arg), void *hook_arg);

#endif
