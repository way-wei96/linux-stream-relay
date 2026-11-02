#ifndef STOP_FLAG_H
#define STOP_FLAG_H

/* 安装 SIGINT 处理器（Ctrl+C）。 */
void stop_flag_install_handlers(void);

/* 是否已收到停止信号。 */
int stop_flag_is_set(void);

#endif
