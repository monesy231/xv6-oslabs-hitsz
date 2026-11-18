# HITSZ 操作系统实验

哈尔滨工业大学（深圳）操作系统课程实验一至五。实验一至四使用 RISC-V xv6 和 QEMU，实验五在 Linux 上通过 FUSE 2 和虚拟磁盘实现用户态文件系统。

[五项实验完整归档](https://github.com/monesy231/xv6-oslabs-hitsz/tree/kernel-and-filesystem)汇集源码、测试和各实验的运行说明。也可以直接切换到实验一至四各自的 xv6 分支。

| 实验 | 内容 | 分支与运行说明 |
| --- | --- | --- |
| 一 · 用户态工具与管道 | `sleep`、父子进程通信 `pingpong`、递归查找 `find` | [`util`](https://github.com/monesy231/xv6-oslabs-hitsz/tree/util) |
| 二 · 进程系统调用 | 退出信息、阻塞与非阻塞等待、主动让出 CPU | [`syscall`](https://github.com/monesy231/xv6-oslabs-hitsz/tree/syscall) |
| 三 · 多核锁与磁盘缓冲 | 每 CPU 空闲页链表、按哈希桶组织磁盘缓冲区 | [`lock`](https://github.com/monesy231/xv6-oslabs-hitsz/tree/lock) |
| 四 · 进程内核页表 | Sv39 页表打印、每进程内核页表、内核读取用户地址 | [`pgtbl`](https://github.com/monesy231/xv6-oslabs-hitsz/tree/pgtbl) |
| 五 · FUSE 用户态文件系统 | 从虚拟磁盘读取目录项、目录与空文件创建、元数据持久化 | [`kernel-and-filesystem/fuse-filesystem`](https://github.com/monesy231/xv6-oslabs-hitsz/tree/kernel-and-filesystem/fuse-filesystem) |

## 获取五项实验

```sh
git clone --branch kernel-and-filesystem https://github.com/monesy231/xv6-oslabs-hitsz.git
cd xv6-oslabs-hitsz
```

归档中的前四项各有独立的 Makefile 和课程评分入口。实验五的编译、挂载和卸载步骤见[实验五说明](https://github.com/monesy231/xv6-oslabs-hitsz/blob/kernel-and-filesystem/fuse-filesystem/README.md)。
