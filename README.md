# linux-stream-relay

通用 Linux 用户态字节流中继程序（C）。插件化；核心与协议无关，原始字节中继，解析为可选插件。

用来练习 Linux 用户态 C：fd IO、`poll`、多线程等（按小步迭代）。

## 当前进度

stdin → stdout 原始字节拷贝（最小中继形态）。

## 编译

```text
cc -Wall -Wextra -O0 -g -o relay src/main.c
```

## 运行示例

```text
echo 'hello' | ./relay
printf '\x01\x02\x03' | ./relay | xxd
```

无协议解析：读到什么字节就写出什么字节。
