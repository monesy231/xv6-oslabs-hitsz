# 实验一 · xv6 用户态工具与管道通信

在 RISC-V xv6 中实现三个用户程序，练习调用内核接口、在父子进程之间传递数据，以及遍历 xv6 文件系统。

## 实现

| 命令 | 行为 | 源码 |
| --- | --- | --- |
| `sleep <ticks>` | 将参数转换为时钟滴答数，调用 `sleep` 系统调用。结束后输出 `Sleep <ticks>`。 | [user/sleep.c](user/sleep.c) |
| `pingpong` | 在 `fork` 前创建两条单向管道，父子进程交换各自的 PID。子进程先打印收到的 ping，父进程收到回复后打印 pong 并回收子进程。 | [user/pingpong.c](user/pingpong.c) |
| `find <path> <name>` | 从指定目录递归查找名称完全匹配的文件和子目录，跳过空目录项、`.` 和 `..`，逐行输出匹配路径。 | [user/find.c](user/find.c) |

[Makefile](Makefile) 把三个程序编译为 xv6 用户程序并写入 `fs.img`。[conf/lab.mk](conf/lab.mk) 选择 `util` 的构建和评分配置。

## 构建和运行

宿主机需要 GNU Make、C 编译器、Perl、RISC-V GCC 和 binutils，以及 `qemu-system-riscv64`。自动评分还需要 `python` 命令指向 Python 3。

```sh
git clone --branch util https://github.com/monesy231/xv6-oslabs-hitsz.git
cd xv6-oslabs-hitsz
make qemu
```

若工具链前缀需要手动指定，可按安装情况设置 `TOOLPREFIX`，例如 `make qemu TOOLPREFIX=riscv64-linux-gnu-`。

在 **xv6 shell** 中执行以下命令。示例使用新建的测试目录。

```sh
sleep 3
pingpong
mkdir demo
echo hello > demo/needle
find . needle
```

预期看到 `Sleep 3`、两行带有父子 PID 的 ping 和 pong，以及路径 `./demo/needle`。PID 由 xv6 在运行时分配。

## 自动验证

先按 `Ctrl-a`，再按 `x` 退出 QEMU，在**宿主机终端**运行评分。

```sh
make grade
```

[评分脚本](grade-lab-util) 检查 `sleep` 的无参数行为和系统调用入口、`pingpong` 的输出顺序和 PID 对应关系，以及 `find` 查找文件、目录和嵌套路径的结果。评分会重新构建文件系统镜像，镜像里的临时文件会被清除。

[实验一源码补丁](https://github.com/monesy231/xv6-oslabs-hitsz/blob/kernel-and-filesystem/xv6-user-tools/docs/submission.patch)

其他实验见 [实验一至五总览](https://github.com/monesy231/xv6-oslabs-hitsz/tree/kernel-and-filesystem)。上游 xv6 背景和版权信息见 [原始 README](README) 与 [LICENSE](LICENSE)。
