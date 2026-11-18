/* main.c源码 */
#define _XOPEN_SOURCE 700

#define FUSE_USE_VERSION 26
#include "stdio.h"
#include "fuse.h"
#include "../include/ddriver.h"
#include <linux/fs.h>
#include "pwd.h"
#include "unistd.h"
#include "string.h"
#include <errno.h>
#include <stdlib.h>

#define DEMO_DEFAULT_PERM        0777


/* 超级块 */
struct demo_super
{
    int     driver_fd;  /* 模拟磁盘的fd */

    int     sz_io;      /* 磁盘IO大小，单位B */
    int     sz_disk;    /* 磁盘容量大小，单位B */
    int     sz_blks;    /* 逻辑块大小，单位B */
};

/* 目录项 */
struct demo_dentry
{
    char    fname[128];
}; 

struct demo_super super = { .driver_fd = -1 };

#define DEVICE_NAME "ddriver"

/* 挂载文件系统 */
static void* demo_mount(struct fuse_conn_info * conn_info){
    (void)conn_info;
    // 打开驱动
    char device_path[4096];
    /* 与 shell 中的 ~/ddriver 一致，也便于用临时 HOME 隔离测试盘。 */
    const char *home = getenv("HOME");
    if (!home || !*home) home = getpwuid(getuid())->pw_dir;
    snprintf(device_path, sizeof(device_path), "%s/" DEVICE_NAME, home);
    super.driver_fd = ddriver_open(device_path);
    if (super.driver_fd < 0) {
        fprintf(stderr, "打开虚拟磁盘失败：%d\n", super.driver_fd);
        exit(EXIT_FAILURE);
    }

    /* IO 块为 512B，一个逻辑块由两个 IO 块组成。 */
    ddriver_ioctl(super.driver_fd, IOC_REQ_DEVICE_IO_SZ, &super.sz_io);
    ddriver_ioctl(super.driver_fd, IOC_REQ_DEVICE_SIZE, &super.sz_disk);
    super.sz_blks = 2 * super.sz_io;

    return 0;
}

/* 卸载文件系统 */
static void demo_umount(void* p){
    (void)p;
    // 关闭驱动
    ddriver_close(super.driver_fd);
    super.driver_fd = -1;
}

/* 从第 500 个逻辑块读取目录项，文件名由测试脚本写入，不能写死。 */
static int demo_read_dentry(struct demo_dentry *dentry)
{
    char block[1024];
    if (super.sz_io != 512 || super.sz_blks != sizeof(block))
        return -EIO;
    if (ddriver_seek(super.driver_fd, 500 * super.sz_blks, SEEK_SET) < 0)
        return -EIO;

    /* 驱动一次只允许读一个 IO 块，分两次读完逻辑块。 */
    for (int i = 0; i < super.sz_blks; i += super.sz_io) {
        if (ddriver_read(super.driver_fd, block + i, super.sz_io) != super.sz_io)
            return -EIO;
    }
    memcpy(dentry, block, sizeof(*dentry));
    dentry->fname[sizeof(dentry->fname) - 1] = '\0';
    return 0;
}

/* 遍历目录 */
static int demo_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi)
{
    (void)path;
    (void)offset;
    (void)fi;
    struct demo_dentry dentry;
    int ret = demo_read_dentry(&dentry);
    if (ret < 0)
        return ret;
    /* 任务一只有一个普通文件，不添加 . 和 ..。 */
    filler(buf, dentry.fname, NULL, 0);
    return 0;
}

/* 显示文件属性 */
static int demo_getattr(const char* path, struct stat *stbuf)
{
    memset(stbuf, 0, sizeof(*stbuf));
    if(strcmp(path, "/") == 0) {
        stbuf->st_mode = DEMO_DEFAULT_PERM | S_IFDIR;            // 根目录是目录文件
        stbuf->st_nlink = 2;
    } else {
        struct demo_dentry dentry;
        int ret = demo_read_dentry(&dentry);
        if (ret < 0)
            return ret;
        if (path[0] != '/' || strcmp(path + 1, dentry.fname) != 0)
            return -ENOENT;
        stbuf->st_mode = DEMO_DEFAULT_PERM | S_IFREG;            // 预设文件是普通文件
        stbuf->st_nlink = 1;
    }
    stbuf->st_uid = getuid();
    stbuf->st_gid = getgid();
    stbuf->st_blksize = super.sz_blks;
    return 0;
}

/* 根据任务1需求 只用实现前四个钩子函数即可完成ls操作 */
static struct fuse_operations ops = {
	.init = demo_mount,						          /* mount文件系统 */		
	.destroy = demo_umount,							  /* umount文件系统 */
	.getattr = demo_getattr,							  /* 获取文件属性 */
	.readdir = demo_readdir,							  /* 填充dentrys */
};

int main(int argc, char *argv[])
{
    int ret = 0;
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    ret = fuse_main(args.argc, args.argv, &ops, NULL);
    fuse_opt_free_args(&args);
    return ret;
}
