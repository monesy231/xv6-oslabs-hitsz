# 从 xv6 内核到 FUSE 文件系统

哈尔滨工业大学（深圳）操作系统课程的五项实验。前四项各有一套可独立构建的 RISC-V xv6 源码，第五项在 Linux 上使用 FUSE 2 和 `ddriver` 虚拟磁盘。

## 实验目录

| 实验 | 实现内容 | 运行与验证 | 实验材料 |
| --- | --- | --- | --- |
| 一 · 用户态工具与管道 | `sleep`、`pingpong`、`find` | [运行指南](xv6-user-tools/README.md) | [源码补丁](xv6-user-tools/docs/submission.patch) |
| 二 · 进程系统调用与调度 | `exit`、`wait`、`yield` | [运行指南](xv6-process-syscalls/README.md) | [设计笔记](xv6-process-syscalls/docs/design-notes.md) · [源码补丁](xv6-process-syscalls/docs/submission.patch) |
| 三 · 多核锁与磁盘缓冲 | 按 CPU 分配物理页、按桶管理缓冲区 | [运行指南](xv6-multicore-locking/README.md) | [实验报告](xv6-multicore-locking/docs/report.md) · [源码补丁](xv6-multicore-locking/docs/submission.patch) |
| 四 · RISC-V 进程内核页表 | Sv39 页表打印、进程专属内核页表、用户地址读取 | [运行指南](riscv-kernel-page-tables/README.md) | [实验报告](riscv-kernel-page-tables/docs/report.md) · [源码补丁](riscv-kernel-page-tables/docs/submission.patch) |
| 五 · FUSE 用户态文件系统 | 虚拟磁盘目录项、目录与空文件、元数据持久化 | [构建与挂载指南](fuse-filesystem/README.md) | [磁盘布局](fuse-filesystem/fs/newfs/include/fs.layout) · [测试脚本](fuse-filesystem/fs/newfs/tests/test.sh) |

## 获取源码

```sh
git clone --branch kernel-and-filesystem https://github.com/monesy231/xv6-oslabs-hitsz.git
cd xv6-oslabs-hitsz
```

已有此仓库副本时，可在 GitHub 页面选择 `kernel-and-filesystem`，或从指向此 GitHub 仓库的远端拉取并切换分支。每个目录已包含可构建的源码。表中的补丁用于在对应的课程基线上复现实验改动，基线见各实验指南。

## 构建与验证

### 实验一至四

准备 GNU Make、宿主机 C 编译器、Perl、RISC-V GCC 和 binutils、`qemu-system-riscv64`，以及用于评分的 Python 3。部分评分脚本通过 `python` 启动，运行前用 `python --version` 确认它指向 Python 3。

进入对应实验目录后运行 `make qemu`。实验二可用 `make qemu CPUS=1` 复现单核调度测试。退出 QEMU 时先按 `Ctrl-a`，松开后再按 `x`，然后在该实验目录运行 `make grade`。若 Makefile 未识别工具链，可按安装情况传入 `TOOLPREFIX=riscv64-linux-gnu-`。各实验的手工测试和附加题状态见上表中的运行指南。

### 实验五

进入 `fuse-filesystem/`，在 Linux 上使用 C 编译器、CMake 和 FUSE 2 开发库构建 `demo` 与 `newfs`。实际挂载需要可访问的 `/dev/fuse` 和 `fusermount`。[实验五指南](fuse-filesystem/README.md)

## 上游代码

各实验保留上游许可证。xv6 快照来自 [HITSZ xv6 课程框架](https://gitee.com/ftutorials/xv6-oslabs-hitsz)，FUSE 快照来自[用户态文件系统课程框架](https://gitee.com/ftutorials/user-land-filesystem)。
