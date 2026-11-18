# 从 xv6 内核到 FUSE 文件系统

哈尔滨工业大学（深圳）操作系统课程的五项实验。前四项各自带有一套可独立构建的 RISC-V xv6 源码，最后一项在 Linux 上使用 FUSE 2 和虚拟磁盘。

## 实验目录

| 实验 | 主题 | 代码与说明 |
| --- | --- | --- |
| 一 | 用户态工具与管道 | [`sleep`、`pingpong`、`find`](xv6-user-tools/) |
| 二 | 进程系统调用与调度 | [`exit`、`wait`、`yield`](xv6-process-syscalls/) · [设计笔记](xv6-process-syscalls/docs/design-notes.md) |
| 三 | 多核锁与磁盘缓冲 | [按 CPU 分配物理页、按桶管理缓冲区](xv6-multicore-locking/) · [实验报告](xv6-multicore-locking/docs/report.md) |
| 四 | RISC-V 进程内核页表 | [Sv39、进程独立内核页表、用户内存读取](riscv-kernel-page-tables/) · [实验报告](riscv-kernel-page-tables/docs/report.md) |
| 五 | FUSE 用户态文件系统 | [虚拟磁盘目录项、目录与空文件、元数据持久化](fuse-filesystem/) |

## 运行

在仓库的分支选择器中选择 `kernel-and-filesystem`。如果已有本仓库的本地副本，可在 Linux 终端执行：

```sh
git fetch origin
git switch kernel-and-filesystem
```

xv6 实验需要 GNU Make、宿主机 C 编译器、Perl、RISC-V GCC 和 binutils、`qemu-system-riscv64`、Python 3。进入任一 xv6 实验目录后使用 `make qemu` 启动，按 `Ctrl-a` 再按 `x` 退出。退出后可运行 `make grade`。若 Makefile 未识别交叉工具链，可按安装情况传入 `TOOLPREFIX=riscv64-linux-gnu-`。每项实验的具体测试命令和补丁见相应目录的 README。

FUSE 实验需要 CMake、C 编译器、FUSE 2 开发库，以及支持实际挂载的 `/dev/fuse`。构建和测试步骤见 [FUSE 用户态文件系统](fuse-filesystem/README.md)。

## 上游代码

各实验保留源码、构建和测试文件、测试镜像以及原有许可证。xv6 快照来自 [HITSZ xv6 课程框架](https://gitee.com/ftutorials/xv6-oslabs-hitsz)，FUSE 快照来自 [用户态文件系统课程框架](https://gitee.com/ftutorials/user-land-filesystem)。虚拟磁盘驱动中的本地构建中间文件和预编译库可从源码重新生成。
