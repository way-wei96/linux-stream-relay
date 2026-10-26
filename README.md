# linux-stream-relay

通用 Linux 用户态字节流中继程序（C）。插件化；核心与协议无关，原始字节中继，解析为可选插件。

用来练习 Linux 用户态 C：fd IO、`poll`、多线程等（按小步迭代）。

## 当前进度

输入/输出都可以是标准流或文件；`-` 表示 stdin / stdout。
输入侧非阻塞 + `poll` 等可读；输出仍阻塞写。只搬原始字节。

## 编译

```text
cc -Wall -Wextra -O0 -g -o relay src/main.c
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
