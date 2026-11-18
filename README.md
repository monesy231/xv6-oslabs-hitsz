# HITSZ xv6 操作系统实验

> **五项实验完整归档**位于 `kernel-and-filesystem` 分支。使用仓库顶部的分支选择器切换，即可查看按主题整理的源码、测试、补丁、报告和构建说明。

基于 HITSZ 课程实验框架的个人 xv6 学习与实现记录。项目使用 C 和 RISC-V 汇编，在 QEMU 中运行 xv6 教学操作系统，围绕用户态工具、系统调用、并发锁与页表管理理解内核的工作方式。

实验代码按分支独立保存。**当前默认分支是 `pgtbl`**，查看其他实验时请切换到对应分支；各分支的实现并未合并为一套统一内核。

## 实验导航

| 分支 | 主题 | 当前代码内容 |
| --- | --- | --- |
| [`util`](https://github.com/monesy231/xv6-oslabs-hitsz/tree/util) | 用户态工具 | `sleep`、父子进程管道通信 `pingpong`、递归目录查找 `find`。 |
| [`syscall`](https://github.com/monesy231/xv6-oslabs-hitsz/tree/syscall) | 系统调用与进程 | 退出时的进程信息输出、带标志参数的 `wait`、非阻塞等待、`yield` 与上下文诊断。 |
| [`lock`](https://github.com/monesy231/xv6-oslabs-hitsz/tree/lock) | 并发与锁 | 按 CPU 管理空闲页、跨 CPU 借用内存页、按哈希桶组织磁盘缓冲区；另含 AI 扩展实验框架。 |
| [`pgtbl`](https://github.com/monesy231/xv6-oslabs-hitsz/tree/pgtbl) | 页表管理 | 页表打印、每进程内核页表、用户映射同步，以及 `copyin` / `copyinstr` 的访问处理。 |
| [`main`](https://github.com/monesy231/xv6-oslabs-hitsz/tree/main) | 课程说明 | 保留上游简要说明，不包含可构建的实验源码。 |

以上是源码内容概览，实验是否满足全部要求应以对应评分脚本的实际输出为准。`util` 当前添加的是表中三个工具；`syscall` 的 `seccomp` 附加题以及 `lock` 中 AI 模型加载、KV 读取和预取的学生实现仍留有待实现部分。

## 页表实验概述

默认分支主要围绕以下实现展开：

- **页表可视化**：递归遍历三级页表，显示索引、地址映射和 `r/w/x/u` 权限，便于观察进程地址空间。
- **每进程内核页表**：创建、切换并回收进程专属内核页表，结合进程分配、调度和退出管理其生命周期。
- **用户映射同步**：在进程创建、地址空间增长和程序加载等路径中同步用户映射，维护用户页表与内核页表之间的关系。
- **用户数据读取**：结合用户页访问权限检查与临时 `SUM` 设置处理 `copyin`、`copyinstr`，关注跨页、保护页和地址边界。

阅读入口：[`kernel/vm.c`](kernel/vm.c)、[`kernel/proc.c`](kernel/proc.c)、[`kernel/proc.h`](kernel/proc.h)、[`kernel/exec.c`](kernel/exec.c)。

## 目录结构

以下为 `pgtbl` 分支的主要目录；其他实验分支的用户程序和评分文件有所不同。

```text
.
├── kernel/              # 内核：进程、虚拟内存、文件系统、设备驱动等
├── user/                # 用户程序、系统调用封装与测试程序
├── mkfs/                # 在宿主机构建 xv6 文件系统镜像的工具
├── conf/lab.mk          # 当前实验配置，本分支为 LAB=pgtbl
├── grade-lab-pgtbl      # 页表实验评分入口
├── gradelib.py          # 评分脚本公共支持代码
├── Makefile             # 构建、QEMU 启动、调试和评分规则
├── Dockerfile           # 基于 Arch Linux 的工具链环境定义
├── README               # 原始 xv6 说明，构建时写入文件系统镜像
├── README.md            # 本项目概述与使用指南
└── LICENSE              # 上游 xv6 版权与使用条件
```

## 快速开始

### 1. 准备环境

在 Linux 或具备相应工具链的开发环境中准备：

- Git、GNU Make、宿主机 C 编译器和 Perl。
- RISC-V GCC 与 binutils，例如 `riscv64-unknown-elf-` 或 `riscv64-linux-gnu-` 工具链。
- 支持 RISC-V 系统模拟的 `qemu-system-riscv64`。
- Python 3；评分脚本使用 `#!/usr/bin/env python`，运行 `make grade` 前需确保 `python` 命令可用并指向 Python 3。
- 调试时还需要支持 RISC-V 的 GDB。

`Makefile` 会尝试识别工具链前缀，也可以通过 `TOOLPREFIX` 手动指定。仓库内的 [`Dockerfile`](Dockerfile) 可供环境搭建时参考。

### 2. 获取代码并启动

私有仓库需要使用具有访问权限的 GitHub 账号完成 Git 认证。

```sh
git clone --branch pgtbl https://github.com/monesy231/xv6-oslabs-hitsz.git
cd xv6-oslabs-hitsz
make qemu
```

如果自动识别工具链失败，可按实际安装的前缀执行，例如：

```sh
make qemu TOOLPREFIX=riscv64-linux-gnu-
```

进入 xv6 shell 后，可以运行 `ls`、`cat README` 等命令。退出 QEMU 时先按 `Ctrl-a`，松开后再按 `x`。

### 3. 查看其他实验

先保存当前源码修改并退出 QEMU，再切换分支，例如：

```sh
make clean
git switch util
make qemu
```

每个实验分支自带对应的 `conf/lab.mk`，应随分支一起使用。`make clean` 会清理构建产物及 `fs.img`，xv6 镜像内的临时文件也会随之清除。

## 验证与调试

### 实验评分

在需要检查的实验分支根目录运行：

```sh
make grade
```

该目标会先清理构建产物，再调用 `grade-lab-$(LAB)`。如需手动指定工具链，应在评分命令中同样传入 `TOOLPREFIX`。评分结果取决于当前分支代码和本地环境；本 README 不记录未经复现的分数。

`pgtbl` 分支也可以在 xv6 shell 中分别运行以下程序，观察具体行为：

```text
kvmtest
stats
usertests
```

`syscall` 分支提供 `make grade-extra`，`lock` 分支提供 `make grade-ai` 作为附加实验入口；这两个入口涉及尚未完成的学生实现。

### GDB 调试

在一个宿主机终端中启动等待调试器连接的 QEMU：

```sh
make qemu-gdb
```

在同一项目目录的另一个终端中运行：

```sh
make gdb
```

需要确保 `Makefile` 选用的 GDB 已安装并支持 RISC-V。启动过程会按项目模板生成 `.gdbinit`。

## 相关仓库与来源

- [xv6-backup](https://github.com/monesy231/xv6-backup)：保存 `util` 阶段的三个用户程序及历史 Makefile，适合对照代码，不具备完整运行环境。
- [HITSZ 课程实验框架上游](https://gitee.com/ftutorials/xv6-oslabs-hitsz)：本项目的课程代码来源。
- [MIT xv6 相关课程资料](https://pdos.csail.mit.edu/6.828/)：原始 xv6 说明中引用的教学资料入口。

原始 xv6 项目背景、作者与致谢保留在 [`README`](README) 中；上游代码的版权及使用条件见 [`LICENSE`](LICENSE)。
