#ifndef FDUTIL_H
#define FDUTIL_H

/* 把 fd 设成非阻塞。成功 0，失败 -1。 */
int set_nonblock(int fd);

/* 若 fd 不是标准流，则 close。 */
void close_if_opened(int fd, int std_fd);

/*
 * path == NULL 或 "-" → stdin；否则 open 只读文件。
 * 成功返回 fd；失败 -1。非 stdin 用完需 close。
 */
int open_input(const char *path);

/*
 * path == NULL 或 "-" → stdout；否则创建/截断文件。
 * 成功返回 fd；失败 -1。非 stdout 用完需 close。
 */
int open_output(const char *path);

#endif
