# xv6 用户态工具与管道通信

课程实验一。在 xv6 用户态实现 `sleep`、`pingpong` 和 `find`，练习系统调用、父子进程通信和目录遍历。

## 实现

- `sleep` 接收时钟滴答数，调用内核的 `sleep` 等待，再输出休眠参数。[user/sleep.c](user/sleep.c)
- `pingpong` 在 `fork` 前创建两条单向管道，父子进程交换 PID。父进程收到回复后回收子进程。[user/pingpong.c](user/pingpong.c)
- `find` 从指定目录递归查找同名文件和目录，跳过空目录项以及 `.`、`..`。[user/find.c](user/find.c)

## 运行与验证

在本目录执行 `make qemu`，然后在 xv6 shell 中运行：

```sh
sleep 3
pingpong
mkdir demo
echo hello > demo/needle
find . needle
```

`sleep` 输出 `Sleep 3`，`pingpong` 依次输出 ping 和 pong，`find` 找到 `./demo/needle`。这些命令已在 QEMU 中验证。按 `Ctrl-a` 再按 `x` 退出 QEMU，在宿主机运行 `make grade` 可调用课程评分脚本。 [grade-lab-util](grade-lab-util)

[源码补丁](docs/submission.patch)基于课程标签 `util-base-26`，可在该标签的独立检出中用 `git apply` 应用。
