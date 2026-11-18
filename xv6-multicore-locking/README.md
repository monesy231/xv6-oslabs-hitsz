# xv6 多核锁与磁盘缓冲

课程实验三。把物理页分配和磁盘缓冲区的共享数据拆分，减少多核并发访问时对同一把锁的争用。

## 实现

- 页分配器为每个 CPU 维护一条空闲页链表。优先使用本核空闲页，本核耗尽时从其他 CPU 的链表借页。[kernel/kalloc.c](kernel/kalloc.c)
- 磁盘缓冲区按块号散列到 31 个桶，每个桶有独立的锁。缓存缺失时用淘汰锁协调缓冲区复用，并在插入前再次查找目标块。[kernel/bio.c](kernel/bio.c)

## 运行与验证

在本目录执行 `make qemu`，然后在 xv6 shell 中运行：

```sh
kalloctest
bcachetest
usertests sbrkmuch
```

以上测试均已在 QEMU 中通过。按 `Ctrl-a` 再按 `x` 退出 QEMU，在宿主机执行 `make grade` 可运行必做评分脚本。 [grade-lab-lock](grade-lab-lock)

仓库包含 AI 访问模式的测试框架，可通过 `make grade-ai` 运行。模型块读取、KV 持久化与异步预取的学生接口仍待实现。[grade-ai](grade-ai) · [user/ai_student_model.c](user/ai_student_model.c) · [user/ai_student_kv.c](user/ai_student_kv.c) · [user/ai_student_prefetch.c](user/ai_student_prefetch.c)

实验报告包含竞争分析、分步实现和原始测试数据。 [实验报告](docs/report.md)

[源码补丁](docs/submission.patch)基于课程提交 `23a0c02e6c3781d6bf17bf962bf98e6a10f415bd`，可在独立检出中用 `git apply` 应用。
