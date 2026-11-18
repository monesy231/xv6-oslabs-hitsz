# HITSZ 操作系统实验 · 实验四 RISC-V 进程内核页表

基于哈尔滨工业大学（深圳）操作系统课程框架，在 QEMU 中运行 RISC-V xv6。本仓库的默认分支 `pgtbl` 是实验四，涵盖 Sv39 页表打印、进程独立内核页表和内核读取用户内存。

**[五项实验完整归档](https://github.com/monesy231/xv6-oslabs-hitsz/tree/kernel-and-filesystem)** 位于 `kernel-and-filesystem` 分支，汇总了各实验的源码、运行说明、测试入口与随附材料。

## 实验导航

| 实验 | 主题 | 阅读入口 |
| --- | --- | --- |
| 一 | 用户态工具与管道 | [`util`](https://github.com/monesy231/xv6-oslabs-hitsz/tree/util) · `sleep`、`pingpong`、`find` |
| 二 | 进程系统调用与调度 | [`syscall`](https://github.com/monesy231/xv6-oslabs-hitsz/tree/syscall) · `exit`、`wait`、`yield` |
| 三 | 多核锁与磁盘缓冲 | [`lock`](https://github.com/monesy231/xv6-oslabs-hitsz/tree/lock) · 按 CPU 分配物理页、按桶管理磁盘缓冲 |
| 四 | RISC-V 进程内核页表 | [`pgtbl`](https://github.com/monesy231/xv6-oslabs-hitsz/tree/pgtbl) · 页表打印、独立内核页表与用户数据读取 |
| 五 | FUSE 用户态文件系统 | [`kernel-and-filesystem/fuse-filesystem`](https://github.com/monesy231/xv6-oslabs-hitsz/tree/kernel-and-filesystem/fuse-filesystem) · 挂载、目录和空文件、元数据持久化 |

前四项实验各自使用对应分支的 xv6 源码和配置。实验五在支持 FUSE 的 Linux 宿主机上运行，环境和构建命令见 [FUSE 实验说明](https://github.com/monesy231/xv6-oslabs-hitsz/blob/kernel-and-filesystem/fuse-filesystem/README.md)。

## 实验四实现

| 模块 | 实现要点 | 代码 |
| --- | --- | --- |
| Sv39 页表打印 | `vmprint` 从 L2 递归遍历有效项，输出索引、物理地址和 `r/w/x/u` 权限，叶子项另显示虚拟地址。PID 1 成功执行 `exec` 后打印。 | [`kernel/vm.c`](kernel/vm.c) · [`kernel/exec.c`](kernel/exec.c) |
| 进程专属内核页表 | 分配进程时建立专属页表并映射内核栈。调度器切换 `satp` 并刷新 TLB，切回调度器时恢复全局内核页表。释放时先断开共享项，再回收页表。 | [`kernel/proc.c`](kernel/proc.c) · [`kernel/proc.h`](kernel/proc.h) |
| 同步用户映射 | 首次进程创建、`fork`、`exec` 和地址空间伸缩时同步 PLIC 地址以下的用户映射。用户页表和进程内核页表通过 96 个 L1 项共享用户 L0 表，保留原有用户页权限。 | [`kernel/vm.c`](kernel/vm.c) · [`kernel/proc.c`](kernel/proc.c) · [`kernel/exec.c`](kernel/exec.c) |
| 读取用户数据 | `copyin` 和 `copyinstr` 逐页检查 `V/U/R` 权限，临时开启 `SUM`，调用框架的新拷贝函数后恢复状态。定长拷贝要求全段可读，字符串拷贝可在下一页不可读之前遇到 NUL 并结束。 | [`kernel/vm.c`](kernel/vm.c) · [`kernel/vmcopyin.c`](kernel/vmcopyin.c) |

## 构建与运行

在 Linux 上准备 Git、GNU Make、宿主机 C 编译器、Perl、RISC-V GCC 与 binutils、`qemu-system-riscv64` 和 Python 3。评分脚本使用 `python` 启动，需让该命令指向 Python 3。[Dockerfile](Dockerfile)可作为工具链配置参考。

```sh
git clone --branch pgtbl https://github.com/monesy231/xv6-oslabs-hitsz.git
cd xv6-oslabs-hitsz
make qemu
```

已克隆仓库时可执行 `git switch pgtbl`，然后在仓库根目录构建。若工具链检测失败，可在构建或评分命令后加 `TOOLPREFIX=riscv64-linux-gnu-`，前缀以实际安装为准。[本分支的实验配置](conf/lab.mk)为 `LAB=pgtbl`。

切换到另一项 xv6 实验前，先保存源码修改并退出 QEMU。若要从干净的构建状态开始，可执行 `make clean`，该命令也会清除 `fs.img` 及镜像内的临时文件。

## 验证与调试

QEMU 启动日志包含 `page table` 及三级索引树。进入 xv6 shell 后可逐条执行下表中的命令。

| 命令 | 检查内容 |
| --- | --- |
| `kvmtest` | 当前进程使用独立的内核页表，成功时显示 `kvmtest: OK` |
| `stats` | 显示 `copyin` 和 `copyinstr` 的调用计数，可连续运行两次观察变化 |
| `usertests` | 用户内存访问及系统回归，全部通过时显示 `ALL TESTS PASSED` |

按 `Ctrl-a`，松开后按 `x` 退出 QEMU，再在宿主机运行 `make grade`。[课程评分脚本](grade-lab-pgtbl)检查页表打印、拷贝计数、进程内核页表和 `usertests`。评分会先清理并重建，包括 xv6 文件系统镜像。

调试时，在一个宿主机终端运行 `make qemu-gdb`，在同一仓库目录的另一终端运行 `make gdb`。GDB 需要支持 RISC-V，项目会按模板生成 `.gdbinit`。

## 报告与来源

- 原理题、实现记录和验证结果见[实验报告](https://github.com/monesy231/xv6-oslabs-hitsz/blob/kernel-and-filesystem/riscv-kernel-page-tables/docs/report.md)与[实现笔记](https://github.com/monesy231/xv6-oslabs-hitsz/blob/kernel-and-filesystem/riscv-kernel-page-tables/docs/implementation-notes.md)，归档中也提供[实验补丁](https://github.com/monesy231/xv6-oslabs-hitsz/blob/kernel-and-filesystem/riscv-kernel-page-tables/docs/submission.patch)。
- 课程框架来自 [HITSZ xv6 实验仓库](https://gitee.com/ftutorials/xv6-oslabs-hitsz)。上游 xv6 的项目背景、作者和使用条款见[原始 README](README)与[许可证](LICENSE)。
