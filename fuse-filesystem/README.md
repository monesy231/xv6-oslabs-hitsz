# 实验五 · FUSE 用户态文件系统

本实验在 Linux 上使用 FUSE 2 和 `ddriver` 虚拟磁盘。任务一 `demo` 从磁盘的指定逻辑块读取文件名，并通过 `ls` 显示目录项。任务二 `newfs` 实现路径查找、目录和空文件创建、属性查询、目录遍历，并在正常卸载时保存元数据供重新挂载使用。

| 内容 | 代码与测试 |
| --- | --- |
| 任务一 `demo` | [FUSE 回调](fs/demo/src/main.c) · [测试镜像](fs/demo/tests/disk2) · [课程测试](fs/demo/tests/test.sh) |
| 任务二 `newfs` | [文件系统实现](fs/newfs/src/newfs.c) · [磁盘结构定义](fs/newfs/include/types.h) · [布局描述](fs/newfs/include/fs.layout) · [课程测试](fs/newfs/tests/test.sh) |
| 用户态虚拟磁盘 | [驱动源码](driver/user_ddriver/ddriver.c) |
| 课程参考资料 | [SimpleFS 示例](fs/simplefs/) · [工程模板](fs/template/) · [上游说明](UPSTREAM.md) · [GPL-3.0](LICENSE) |

## 环境与构建

使用 Linux、C 编译器、CMake 和 FUSE 2 开发库。构建前可在 Debian 或 Ubuntu 上准备 `build-essential`、`cmake`、`libfuse-dev` 和 `fuse`。实际挂载需要当前用户可访问 `/dev/fuse`，并能运行 `fusermount`。课程测试还需 Python 3。

在本分支的 `fuse-filesystem/` 目录执行。两个 CMake 工程均从源码构建并链接用户态驱动，已有的 `fs/demo/` 和 `fs/newfs/` 可直接编译。

```bash
cmake -S fs/demo -B fs/demo/build
cmake --build fs/demo/build
cmake -S fs/newfs -B fs/newfs/build
cmake --build fs/newfs/build
```

## 任务一 · 从磁盘读出目录项

`demo` 打开 `$HOME/ddriver`。驱动的 I/O 单位为 512 B，文件系统逻辑块为 1 KiB。`readdir` 从 `500 × 1024` 字节处发起两次读取，取出以 NUL 结尾的文件名。`getattr` 为根目录和该文件返回属性。

在临时目录中复制并修改镜像，挂载后确认 `ls` 输出 `example.txt`。示例只写该镜像副本。

```bash
test_home=$(mktemp -d)
mkdir "$test_home/mnt"
cp fs/demo/tests/disk2 "$test_home/ddriver"
printf 'example.txt\0' | dd of="$test_home/ddriver" bs=1024 seek=500 conv=notrunc status=none

HOME="$test_home" fs/demo/build/demo "$test_home/mnt"
ls "$test_home/mnt"
fusermount -u "$test_home/mnt"
```

## 任务二 · 创建文件并重新挂载

`newfs` 使用 4 MiB 虚拟磁盘。首次挂载新镜像时，建立超级块、inode 位图、数据位图、inode 表和数据区，根目录使用 inode 0。

| 区域 | 逻辑块号 | 用途 |
| --- | --- | --- |
| 超级块 | 0 | 魔数、布局、根目录 inode 编号 |
| inode 位图 | 1 | 记录已分配的 inode |
| 数据块位图 | 2 | 记录已分配的数据块 |
| inode 表 | 3–34 | 512 个 64 B 的 inode |
| 数据区 | 35–4095 | 存放父目录的子项名称和 inode 编号 |

`mkdir` 和 `mknod` 逐段解析父路径，分配 inode，再把目录项加入父目录。父目录按需分配数据块，每个逻辑块存放 7 个目录项，每个目录最多使用 6 个直接块。空文件自身不占数据块。卸载时依次写回目录块、inode 表、位图和超级块，重挂载时校验并恢复这些结构。

继续在同一终端执行。`--device` 指向另一张临时镜像。`-f` 让文件系统进程保持在前台模式，脚本将它放到后台并用 `wait` 等待卸载写回完成。

```bash
mount_newfs() {
    fs/newfs/build/newfs -f --device="$test_home/newfs.img" "$test_home/mnt" &
    newfs_pid=$!
    for attempt in $(seq 1 50); do
        mountpoint -q "$test_home/mnt" && return 0
        kill -0 "$newfs_pid" 2>/dev/null || return 1
        sleep 0.1
    done
    return 1
}

mount_newfs &&
mkdir "$test_home/mnt/docs" &&
touch "$test_home/mnt/docs/notes.txt" &&
ls "$test_home/mnt/docs" &&
fusermount -u "$test_home/mnt" &&
wait "$newfs_pid" &&
mount_newfs &&
test -f "$test_home/mnt/docs/notes.txt" &&
ls "$test_home/mnt/docs" &&
fusermount -u "$test_home/mnt" &&
wait "$newfs_pid"
```

两次 `ls` 均应显示 `notes.txt`。确认已卸载后，可删除 `test_home` 所指的临时目录。

## 课程测试

任务一的课程脚本会把随机文件名写入测试镜像 `disk2`，挂载后检查 `ls` 输出。任务二选择 `N` 运行基础测试，脚本会调用 `ddriver -r` 清空 `$HOME/ddriver`，并用 `umount` 卸载文件系统。

在 Git 检出的 `fuse-filesystem/` 目录执行。以下命令使用当前提交的临时副本，并为两项测试分别准备虚拟磁盘。

```bash
course_workspace=$(mktemp -d)
git -C .. archive HEAD fuse-filesystem | tar -x -C "$course_workspace"
mkdir "$course_workspace/demo-home" "$course_workspace/newfs-home"
cd "$course_workspace/fuse-filesystem"

(
    cd fs/demo/tests
    env HOME="$course_workspace/demo-home" bash test.sh
)

env HOME="$course_workspace/newfs-home" DDRIVER_TYPE=u \
    PATH="$PWD/driver/user_ddriver/bin:$PATH" bash fs/newfs/tests/test.sh
# 出现提示后输入 N
```

若任务二报 `ddriver: command not found`，检查 `PATH` 是否包含本副本的 `driver/user_ddriver/bin`。清盘失败会让前面用例分配的 inode 留在镜像中，造成位图检查失败。

## 验证结果

2026-11-18 在 Linux 5.15、FUSE 2.9.9 环境中，用归档源码的独立副本和临时镜像实际挂载并复跑了两项课程测试。

| 任务 | 课程测评结果 | 验证内容 |
| --- | --- | --- |
| `demo` | `Test Pass!!!` | 读取第 500 个逻辑块，挂载后的 `ls` 显示随机写入的文件名，并正常卸载 |
| `newfs`（`N` 基础测试） | `Score: 30/30` | 挂载、创建多级目录和空文件、列目录、卸载后重挂载，以及 inode 和数据块位图检查 |

手工复验时，卸载并重新挂载同一张临时镜像后，`docs/` 中仍可列出 `notes.txt`。

## 实现范围

本分支的 `newfs` 覆盖必做项目，普通文件内容读写、删除、重命名、硬链接和崩溃恢复未实现。进阶题的 CheckpointFS 和 VectorFS 框架见[课程模板](fs/template/bonus/README.md)。

课程代码源自 HITSZ [用户态文件系统实验框架](https://gitee.com/ftutorials/user-land-filesystem)。从[实验总览](../README.md)可查看本分支收录的其他四项实验。
