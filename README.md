# linux-stream-relay

通用 Linux 用户态字节流中继程序（C）。插件化；核心与协议无关，原始字节中继，解析为可选插件。

用来练习 Linux 用户态 C：fd IO、`poll`、多线程等（按小步迭代）。

## 当前进度

输入/输出都可以是标准流或文件；`-` 表示 stdin / stdout。
读线程 + 写线程 + 有界 ring buffer（`mutex`/`cond`）；两端阻塞 IO。
信号半步：`stop_flag` 接 `SIGINT`（Ctrl+C），读侧看到后 `mark_producer_eof` 收工；
写侧继续把环内剩余数据写完（优雅停不中断 `write`）。
ring 等待用 `pthread_cond_timedwait`：读侧等空位时定期醒、可看见停。
读侧 `read` 前用短超时 `poll` 等可读，避免长期堵在阻塞 `read` 上看不见停。
源码：`main.c`、`fdutil`、`ringbuf`、`relay`、`stop_flag`。

## 编译

```text
cc -Wall -Wextra -O0 -g -pthread -o relay \
  src/main.c src/fdutil.c src/ringbuf.c src/relay.c src/stop_flag.c
```

## 运行示例

```text
echo 'hello' | ./relay
./relay some.bin
./relay some.bin out.bin
echo 'hello' | ./relay - out.bin
./relay some.bin | xxd
```

无协议解析：读到什么字节就写出什么字节。
