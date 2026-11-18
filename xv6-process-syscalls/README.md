# xv6 进程系统调用与调度

课程实验二。围绕进程退出、等待与主动让出 CPU，观察用户程序怎样经由系统调用进入内核。

## 实现

- `exit` 在回收流程中输出退出进程的父进程和子进程信息，便于观察进程状态与父子关系。[kernel/proc.c](kernel/proc.c)
- `wait(status, flags)` 先检查并回收已退出的子进程。实验约定 `flags = 1` 时没有可回收子进程便立即返回 `-1`，`flags = 0` 时可阻塞等待。[kernel/proc.c](kernel/proc.c)
- `yield` 输出内核上下文存储范围、当前进程及候选进程的 PID 和用户程序计数器，再进入调度器。[kernel/sysproc.c](kernel/sysproc.c)

## 运行与验证

在本目录执行 `make qemu CPUS=1`，与评分脚本针对 `yield` 的单核配置一致。在 xv6 shell 中运行：

```sh
exittest
waittest
yieldtest
```

这三项测试已在单核 QEMU 中验证。按 `Ctrl-a` 再按 `x` 退出 QEMU，在宿主机执行 `make grade` 可运行必做评分脚本。 [grade-lab-syscall](grade-lab-syscall)

附加题保留了 `seccomp` 的用户态声明和测试，内核实现待补充。对应评分入口为 `make grade-extra`。[user/seccomptest.c](user/seccomptest.c)

设计笔记记录了调用链和进程状态的处理。[设计笔记](docs/design-notes.md)

[源码补丁](docs/submission.patch)基于课程标签 `syscall-26-fall-v2`，可在该标签的独立检出中用 `git apply` 应用。
