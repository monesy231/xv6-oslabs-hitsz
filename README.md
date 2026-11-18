# 实验三 · xv6 多核锁与磁盘缓冲

哈尔滨工业大学（深圳）操作系统实验三。在 RISC-V xv6 中，物理页分配和磁盘缓冲区是多个 CPU 共享的资源。本实验将两处共享状态拆分，让不同 CPU 的常见操作尽量走各自的锁，同时保留页回收和磁盘块缓存的一致性。五项实验的源码、补丁和报告见[实验总览](https://github.com/monesy231/xv6-oslabs-hitsz/tree/kernel-and-filesystem)。

## 实现

| 模块 | 设计 | 代码 |
| --- | --- | --- |
| 物理页分配器 | 每个 CPU 一条空闲页链表和一把自旋锁。启动时分散空闲页，运行时优先从本核分配。本核用尽时从其他链表一次最多借 32 页，取一页返回并把剩余页留给本核。 | [`kernel/kalloc.c`](kernel/kalloc.c) |
| 磁盘块缓存 | 按块号散列到 31 个桶，各桶的自旋锁保护链表和引用计数，块内数据仍由睡眠锁保护。未命中时经淘汰锁协调回收并再次查找，确保每个设备块只有一份缓存。 | [`kernel/bio.c`](kernel/bio.c) |

页分配器借页时先释放来源锁，再加本核锁。磁盘缓存只复用引用计数为零的缓冲块，普通未命中先扫描一轮，找不到可用块才按桶号顺序加锁复查。磁盘缓冲块数量仍由 `NBUF` 决定。

## 构建与运行

在 Linux 上准备 Git、GNU Make、宿主机 C 编译器、Perl、RISC-V GCC 与 binutils、`qemu-system-riscv64` 和 Python 3。仓库的 [Dockerfile](Dockerfile) 也可作为工具链配置参考。

```sh
git clone --branch lock https://github.com/monesy231/xv6-oslabs-hitsz.git
cd xv6-oslabs-hitsz
make qemu
```

已克隆仓库时可执行 `git switch lock`，然后在仓库根目录构建。[实验配置](conf/lab.mk)选用 `LAB=lock`，QEMU 默认启用三个 CPU。若工具链检测失败，可在构建或评分命令后加 `TOOLPREFIX=riscv64-linux-gnu-`，前缀以实际安装为准。

## 必做测试

在 xv6 shell 中逐条运行以下命令，观察锁竞争统计和测试结果。

| 命令 | 检查内容 |
| --- | --- |
| `kalloctest` | 页分配锁竞争与空闲页数量 |
| `bcachetest` | 缓冲锁竞争、跨块回收及数据正确性 |
| `usertests sbrkmuch` | 大量用户内存的申请和回收 |
| `usertests` | 进程、内存和文件系统回归 |

按 `Ctrl-a`，松开后按 `x` 退出 QEMU，再在宿主机运行 `make grade`。该命令清理构建产物，重建测试镜像，并执行[必做评分脚本](grade-lab-lock)。

## AI 访问负载附加题

附加题用 xv6 用户程序模拟模型分片、embedding 和 KV cache 的文件访问。在 xv6 shell 中先执行 `modelprep` 生成测试数据，再执行 `aiinfer baseline -w 3 -r 24` 运行基线。宿主机上的 `make grade-ai` 单独运行[附加题评分](grade-ai)。

仓库提供基线实现和测试框架。学生侧的[模型与 embedding 加载](user/ai_student_model.c)、[KV 持久化与恢复](user/ai_student_kv.c)和[异步预取](user/ai_student_prefetch.c)仍有待实现。

## 报告与来源

- 锁粒度、加锁顺序和测试记录见[实验报告](https://github.com/monesy231/xv6-oslabs-hitsz/blob/kernel-and-filesystem/xv6-multicore-locking/docs/report.md)，归档中也提供[实验补丁](https://github.com/monesy231/xv6-oslabs-hitsz/blob/kernel-and-filesystem/xv6-multicore-locking/docs/submission.patch)。
- 课程框架来自 [HITSZ xv6 实验仓库](https://gitee.com/ftutorials/xv6-oslabs-hitsz)。上游项目介绍和使用条款见[原始 README](README)与[许可证](LICENSE)。
