# RISC-V 进程内核页表

在 xv6 上实现 Sv39 页表打印、进程专属内核页表以及内核对用户地址的读取。

## 实现内容

- `vmprint` 递归遍历三级页表，输出页表项索引、虚拟地址、物理地址和 `r/w/x/u` 权限。初始进程执行 `exec` 时会打印页表。 [`kernel/vm.c`](kernel/vm.c) · [`kernel/exec.c`](kernel/exec.c)
- 进程分配时创建内核页表并映射内核栈。调度器切换 `satp`，进程释放时回收专属页表。 [`kernel/proc.c`](kernel/proc.c) · [`kernel/proc.h`](kernel/proc.h)
- `fork`、`exec` 和地址空间伸缩同步用户映射。`copyin` 与 `copyinstr` 检查跨页访问的用户权限，按需设置 `SUM` 并在读取后恢复。 [`kernel/proc.c`](kernel/proc.c) · [`kernel/exec.c`](kernel/exec.c) · [`kernel/vm.c`](kernel/vm.c)

## 构建与验证

在本目录使用 GNU Make、宿主机 C 编译器、Perl、RISC-V GCC 与 binutils、`qemu-system-riscv64`，以及 `python` 指向 Python 3 的环境。

```sh
make qemu
```

在 xv6 shell 中可运行 `kvmtest`、`stats` 或 `usertests`。按 `Ctrl-a`，松开后再按 `x` 可退出 QEMU。在宿主机执行下列命令运行课程评分。

```sh
make grade
```

在此源码快照上运行课程评分得到 **100/100**，涵盖页表打印、进程内核页表、`copyin` 与 `copyinstr` 调用计数，以及完整的 `usertests`。 [`grade-lab-pgtbl`](grade-lab-pgtbl)

## 源码与原交付补丁

源码来自 `xv6-oslabs-hitsz` 的 `pgtbl` 快照 `f4f6296d4c5442809fc736bc3055873d6b1e62ca`，保留上游 xv6 的 [许可证](LICENSE) 和 [原始说明](README)。

原交付补丁以课程标签 `pgtbl-base-26`（`6e312f7efdf97cd3553007f30943bbfd5b0018ef`）为基线，记录本实验六个内核文件的变更。在基线的独立检出中执行 `git apply --check /path/to/submission.patch` 可检查补丁，再执行 `git apply /path/to/submission.patch` 可还原实现。本目录已经包含对应源码。 [原交付补丁](docs/submission.patch)

设计细节、原理题和验证记录见 [实验报告](docs/report.md) 与 [实现过程](docs/implementation-notes.md)。
