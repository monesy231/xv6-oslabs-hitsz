# 实验二 · xv6 进程系统调用与调度

本实验扩展 xv6 的进程退出、等待和调度接口。用户态调用经 `ecall` 进入内核，由系统调用分发表选择处理函数，参数来自陷阱现场保存的寄存器，返回值写回 `a0`。入口见 [user/usys.pl](user/usys.pl) 和 [kernel/syscall.c](kernel/syscall.c)。

## 实现

| 接口 | 行为 | 源码 |
| --- | --- | --- |
| `exit(status)` | 输出父进程及子进程的 PID、名称和状态。对子进程的记录在将其移交给 `init` 之前生成。退出进程进入 `ZOMBIE`，等待父进程回收。 | [kernel/proc.c](kernel/proc.c) |
| `wait(status, flags)` | 优先回收已经退出的子进程，写回退出状态并返回该子进程的 PID。`flags = 0` 可阻塞等待，`flags = 1` 在无已退出子进程时返回 `-1`。状态参数可传空指针以省略写回。 | [kernel/proc.c](kernel/proc.c)、[kernel/sysproc.c](kernel/sysproc.c) |
| `yield()` | 输出内核上下文的存储范围、当前进程与就绪候选进程的 PID 和用户程序计数器，再让出 CPU。评分脚本使用单核配置观察调度顺序。 | [kernel/sysproc.c](kernel/sysproc.c)、[kernel/proc.h](kernel/proc.h) |

用户态 `printf` 使用 128 字节缓冲合并短消息的写入，控制台在持锁期间同步发送到 UART，以减少并发输出逐字交错。长消息仍可能分段发送。[user/printf.c](user/printf.c) · [kernel/console.c](kernel/console.c)

## 构建和运行

宿主机需要 GNU Make、C 编译器、Perl、RISC-V GCC 和 binutils，以及 `qemu-system-riscv64`。自动评分还需要 `python` 命令指向 Python 3。若 Makefile 未识别交叉工具链，请根据本机安装为 `make` 传入工具链前缀，例如 `TOOLPREFIX=riscv64-linux-gnu-`。

```sh
git clone --branch syscall https://github.com/monesy231/xv6-oslabs-hitsz.git
cd xv6-oslabs-hitsz
make qemu CPUS=1
```

进入 **xv6 shell** 后依次运行三项测试。

```sh
exittest
waittest
yieldtest
```

| 测试 | 观察点 |
| --- | --- |
| [exittest](user/exittest.c) | 进程退出时打印父子关系和进程状态。 |
| [waittest](user/waittest.c) | 先进行非阻塞检查，再阻塞等待子进程退出。测试成功时输出 `wait test OK`。 |
| [yieldtest](user/yieldtest.c) | 父进程调用 `yield`，观察上下文范围、进程信息和子进程开始运行的顺序。 |

## 自动验证

按 `Ctrl-a`，再按 `x` 退出 QEMU，回到**宿主机终端**运行评分。

```sh
make grade
```

[必做评分脚本](grade-lab-syscall) 检查退出日志、阻塞和非阻塞等待，以及调度诊断。脚本为评分启用 `LAB_SYSCALL_TEST`，并使用交叉工具链的 `addr2line` 检查 `yield` 输出的程序计数器对应的函数。

系统调用过滤附加题保留了用户态接口和测试程序，内核处理尚待实现。该部分单独通过 `make grade-extra` 调用。[user/seccomptest.c](user/seccomptest.c) · [附加题评分脚本](grade-lab-syscall-extra)

[实验二设计笔记](https://github.com/monesy231/xv6-oslabs-hitsz/blob/kernel-and-filesystem/xv6-process-syscalls/docs/design-notes.md) · [源码补丁](https://github.com/monesy231/xv6-oslabs-hitsz/blob/kernel-and-filesystem/xv6-process-syscalls/docs/submission.patch)

其他实验见 [实验一至五总览](https://github.com/monesy231/xv6-oslabs-hitsz/tree/kernel-and-filesystem)。上游 xv6 背景和版权信息见 [原始 README](README) 与 [LICENSE](LICENSE)。
