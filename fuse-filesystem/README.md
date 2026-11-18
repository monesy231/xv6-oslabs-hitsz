# FUSE 用户态文件系统

通过 FUSE 2 和 `ddriver` 虚拟磁盘完成两项必做任务。`demo` 从第 500 个逻辑块读取文件名，通过 `ls` 显示目录项。`newfs` 实现路径查找、目录与空文件创建、属性查询和目录遍历，并在卸载时保存元数据供重新挂载使用。

| 组成 | 入口 |
| --- | --- |
| 磁盘目录项演示 | [源码](fs/demo/src/main.c) · [测试脚本](fs/demo/tests/test.sh) · [测试镜像](fs/demo/tests/disk2) |
| `newfs` 文件系统 | [源码](fs/newfs/src/newfs.c) · [磁盘布局](fs/newfs/include/fs.layout) · [测试脚本](fs/newfs/tests/test.sh) |
| 用户态虚拟磁盘 | [驱动源码](driver/user_ddriver/ddriver.c) |
| 课程基线 | [参考实现](fs/simplefs/) · [课程模板](fs/template/) · [上游说明](UPSTREAM.md) · [GPL-3.0](LICENSE) |

## 编译

使用 Linux、C 编译器、CMake 和 FUSE 2 开发库。在 Debian 或 Ubuntu 上可安装 `build-essential`、`cmake`、`libfuse-dev` 和 `fuse`。挂载需要可用的 `/dev/fuse` 和 `fusermount`。在本目录运行以下命令，两个项目都会从源码编译所需的用户态驱动。

```bash
cmake -S fs/demo -B fs/demo/build
cmake --build fs/demo/build
cmake -S fs/newfs -B fs/newfs/build
cmake --build fs/newfs/build
```

## 验证必做功能

把测试镜像复制到临时目录，再将 `example.txt` 写入偏移 `500 × 1024`。挂载后用 `ls` 确认目录项。

```bash
test_home=$(mktemp -d)
mkdir "$test_home/mnt"
cp fs/demo/tests/disk2 "$test_home/ddriver"
printf 'example.txt\0' | dd of="$test_home/ddriver" bs=1024 seek=500 conv=notrunc status=none
HOME="$test_home" fs/demo/build/demo "$test_home/mnt"
ls "$test_home/mnt"
fusermount -u "$test_home/mnt"
```

使用另一张磁盘测试 `newfs`。第二次挂载时应仍能看到 `notes.txt`。

```bash
fs/newfs/build/newfs --device="$test_home/newfs.img" "$test_home/mnt"
mkdir "$test_home/mnt/docs"
touch "$test_home/mnt/docs/notes.txt"
ls "$test_home/mnt/docs"
fusermount -u "$test_home/mnt"

fs/newfs/build/newfs --device="$test_home/newfs.img" "$test_home/mnt"
ls "$test_home/mnt/docs"
fusermount -u "$test_home/mnt"
```

本次在 Linux 上编译了两个目标，并通过回调测试验证了 `demo` 从指定逻辑块读出目录项，以及 `newfs` 创建目录和空文件、列目录、卸载后重新加载元数据。整理环境缺少 `/dev/fuse`，因此实际 FUSE 挂载步骤需在支持 FUSE 的环境中运行。

`newfs` 的实现范围是课程必做功能。普通文件内容读写属于进阶功能，CheckpointFS 和 VectorFS 的选做素材见[课程模板](fs/template/bonus/README.md)。

归档保留了原有作者署名和许可证。联系邮箱、固定到特定主机内核版本的编辑器配置和本地构建产物已从归档中移除。
