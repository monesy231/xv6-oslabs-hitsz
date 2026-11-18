#define _XOPEN_SOURCE 700
#include "newfs.h"
#include <stddef.h>

static struct newfs_super super = {.fd = -1};
static struct custom_options options;

_Static_assert(sizeof(struct newfs_inode_d) == 64, "inode must be 64 bytes");

/* 任意范围读写拆成 512B IO；部分写入使用“读—改—写”。 */
static int disk_io(int offset, void *data, int size, int write_mode)
{
    char block[512];
    char *bytes = data;
    if (offset < 0 || size < 0 || offset > super.disk_size - size)
        return -EINVAL;
    while (size > 0) {
        int start = offset / super.io_size * super.io_size;
        int skip = offset - start;
        int count = super.io_size - skip;
        if (count > size) count = size;
        if (!write_mode || skip != 0 || count != super.io_size) {
            if (ddriver_seek(super.fd, start, SEEK_SET) < 0 ||
                ddriver_read(super.fd, block, super.io_size) != super.io_size)
                return -EIO;
        }
        if (write_mode) {
            memcpy(block + skip, bytes, count);
            if (ddriver_seek(super.fd, start, SEEK_SET) < 0 ||
                ddriver_write(super.fd, block, super.io_size) != super.io_size)
                return -EIO;
        } else {
            memcpy(bytes, block + skip, count);
        }
        offset += count;
        bytes += count;
        size -= count;
    }
    return 0;
}

/* 位图中为 0 的位表示空闲，从前向后找一个即可。 */
static int alloc_bit(uint8_t *map, int limit)
{
    for (int i = 0; i < limit; i++) {
        if (!(map[i / 8] & (1u << (i % 8)))) {
            map[i / 8] |= 1u << (i % 8);
            return i;
        }
    }
    return -ENOSPC;
}

static void free_bit(uint8_t *map, int i)
{
    map[i / 8] &= ~(1u << (i % 8));
}

/* 每个目录最多 42 项，每个数据块放 7 项，余下字节补零。 */
static int sync_directories(int write_mode)
{
    for (int ino = 0; ino < NEWFS_INODE_COUNT; ino++) {
        if (!(super.inode_map[ino / 8] & (1u << (ino % 8)))) continue;
        struct newfs_inode_d *dir = &super.inodes[ino];
        if (dir->type != S_IFDIR) continue;
        for (unsigned int b = 0; b < dir->block_count; b++) {
            unsigned int relative = dir->direct[b];
            if (relative >= super.disk.data_blks ||
                !(super.data_map[relative / 8] & (1u << (relative % 8))))
                return -EIO;
            unsigned int first = b * NEWFS_DENTRY_PER_BLOCK;
            unsigned int count = dir->dir_count > first ? dir->dir_count - first : 0;
            if (count > NEWFS_DENTRY_PER_BLOCK) count = NEWFS_DENTRY_PER_BLOCK;
            char block[NEWFS_BLOCK_SIZE] = {0};
            int size = count * sizeof(struct newfs_dentry_d);
            if (write_mode) memcpy(block, &super.entries[ino][first], size);
            int ret = disk_io(super.disk.data_offset + relative * NEWFS_BLOCK_SIZE,
                              block, sizeof(block), write_mode);
            if (ret < 0) return ret;
            if (!write_mode) memcpy(&super.entries[ino][first], block, size);
        }
        /* 重挂载时检查名字和 inode 引用，避免使用坏目录项。 */
        if (!write_mode) {
            for (unsigned int i = 0; i < dir->dir_count; i++) {
                struct newfs_dentry_d *entry = &super.entries[ino][i];
                if (!memchr(entry->name, '\0', MAX_NAME_LEN) ||
                    entry->ino >= NEWFS_INODE_COUNT ||
                    !(super.inode_map[entry->ino / 8] & (1u << (entry->ino % 8))))
                    return -EIO;
            }
        }
    }
    return 0;
}

/* 在一个目录中查找名字，返回 inode 编号。 */
static int find_child(int parent, const char *name)
{
    struct newfs_inode_d *dir = &super.inodes[parent];
    if (dir->type != S_IFDIR) return -ENOTDIR;
    for (unsigned int i = 0; i < dir->dir_count; i++) {
        struct newfs_dentry_d *entry = &super.entries[parent][i];
        if (!strcmp(entry->name, name)) return entry->ino;
    }
    return -ENOENT;
}

/* 从根目录开始逐段解析 /a/b/c，不创建 . 和 .. 特殊目录项。 */
static int lookup(const char *path)
{
    char copy[4096], *save;
    int ino = super.disk.root_ino;
    if (super.fd < 0) return -EIO;
    if (!path || path[0] != '/') return -EINVAL;
    if (strlen(path) >= sizeof(copy)) return -ENAMETOOLONG;
    strcpy(copy, path);
    for (char *name = strtok_r(copy, "/", &save); name;
         name = strtok_r(NULL, "/", &save)) {
        if (strlen(name) >= MAX_NAME_LEN) return -ENAMETOOLONG;
        ino = find_child(ino, name);
        if (ino < 0) return ino;
    }
    if (path[strlen(path) - 1] == '/' && super.inodes[ino].type != S_IFDIR)
        return -ENOTDIR;
    return ino;
}

/* 卸载时先写目录数据，再将 inode 表、两张位图和超级块刷回磁盘。 */
static int sync_metadata(void)
{
    int ret = sync_directories(1);
    if (ret < 0) return ret;
    ret = disk_io(super.disk.ino_offset, super.inodes, sizeof(super.inodes), 1);
    if (ret < 0) return ret;
    ret = disk_io(super.disk.ino_map_offset, super.inode_map, sizeof(super.inode_map), 1);
    if (ret < 0) return ret;
    ret = disk_io(super.disk.data_map_offset, super.data_map, sizeof(super.data_map), 1);
    if (ret < 0) return ret;
    return disk_io(0, &super.disk, sizeof(super.disk), 1);
}

int newfs_mount(const char *device)
{
    if (super.fd >= 0) return -EBUSY;
    memset(&super, 0, sizeof(super));
    super.fd = ddriver_open((char *)device);
    if (super.fd < 0) return super.fd;
    int ret = -EIO;
    if (ddriver_ioctl(super.fd, IOC_REQ_DEVICE_IO_SZ, &super.io_size) < 0 ||
        ddriver_ioctl(super.fd, IOC_REQ_DEVICE_SIZE, &super.disk_size) < 0 ||
        super.io_size != 512 || super.disk_size != 4 * 1024 * 1024)
        goto fail;
    ret = disk_io(0, &super.disk, sizeof(super.disk), 0);
    if (ret < 0) goto fail;
    if (super.disk.magic != NEWFS_MAGIC) {
        /* 首次挂载：五区依次排列，根目录使用 inode 0。 */
        super.disk = (struct newfs_super_d) {
            .magic = NEWFS_MAGIC, .blk_size = NEWFS_BLOCK_SIZE,
            .sb_offset = 0, .sb_blks = 1,
            .ino_map_offset = 1024, .ino_map_blks = 1,
            .data_map_offset = 2048, .data_map_blks = 1,
            .ino_offset = 3072, .ino_blks = NEWFS_INODE_BLOCKS,
            .data_offset = NEWFS_DATA_START * NEWFS_BLOCK_SIZE,
            .data_blks = super.disk_size / NEWFS_BLOCK_SIZE - NEWFS_DATA_START,
            .root_ino = 0, .max_inodes = NEWFS_INODE_COUNT
        };
        super.inode_map[0] = 1;
        super.inodes[0] = (struct newfs_inode_d) {
            .ino = 0, .type = S_IFDIR, .atime = time(NULL), .mtime = time(NULL)
        };
        ret = sync_metadata();
    } else {
        /* 检查布局，再恢复位图和 inode 表。 */
        if (super.disk.blk_size != NEWFS_BLOCK_SIZE ||
            super.disk.sb_offset != 0 || super.disk.sb_blks != 1 ||
            super.disk.ino_map_offset != 1024 || super.disk.ino_map_blks != 1 ||
            super.disk.data_map_offset != 2048 || super.disk.data_map_blks != 1 ||
            super.disk.ino_offset != 3072 || super.disk.ino_blks != NEWFS_INODE_BLOCKS ||
            super.disk.data_offset != NEWFS_DATA_START * NEWFS_BLOCK_SIZE ||
            super.disk.data_blks != (unsigned int)(super.disk_size / NEWFS_BLOCK_SIZE - NEWFS_DATA_START) ||
            super.disk.root_ino != 0 || super.disk.max_inodes != NEWFS_INODE_COUNT) {
            ret = -EINVAL;
            goto fail;
        }
        ret = disk_io(super.disk.ino_map_offset, super.inode_map, sizeof(super.inode_map), 0);
        if (ret < 0) goto fail;
        ret = disk_io(super.disk.data_map_offset, super.data_map, sizeof(super.data_map), 0);
        if (ret < 0) goto fail;
        ret = disk_io(super.disk.ino_offset, super.inodes, sizeof(super.inodes), 0);
        if (ret < 0) goto fail;
        /* 目录项数和直接块数不能超出固定数组。 */
        for (int i = 0; i < NEWFS_INODE_COUNT; i++) {
            if (!(super.inode_map[i / 8] & (1u << (i % 8)))) continue;
            struct newfs_inode_d *node = &super.inodes[i];
            if (node->ino != (unsigned int)i ||
                (node->type != S_IFDIR && node->type != S_IFREG) ||
                node->block_count > NEWFS_DIRECT_COUNT ||
                node->dir_count > node->block_count * NEWFS_DENTRY_PER_BLOCK) {
                ret = -EIO;
                goto fail;
            }
        }
        if (!(super.inode_map[0] & 1) || super.inodes[0].type != S_IFDIR) {
            ret = -EIO;
            goto fail;
        }
        ret = sync_directories(0);
    }
    if (ret == 0) return 0;
fail:
    ddriver_close(super.fd);
    super.fd = -1;
    return ret;
}

int newfs_umount(void)
{
    if (super.fd < 0) return -EINVAL;
    int ret = sync_metadata();
    int close_ret = ddriver_close(super.fd);
    super.fd = -1;
    return ret < 0 ? ret : close_ret;
}

/* mkdir 和 mknod 共用创建逻辑，只是 inode 类型不同。 */
static int create_node(const char *path, uint32_t type)
{
    char parent_path[4096];
    if (!path || path[0] != '/') return -EINVAL;
    if (strlen(path) >= sizeof(parent_path)) return -ENAMETOOLONG;
    strcpy(parent_path, path);
    size_t len = strlen(parent_path);
    while (len > 1 && parent_path[len - 1] == '/') parent_path[--len] = '\0';
    if (len == 1) return -EEXIST;
    char *name = strrchr(parent_path, '/') + 1;
    if (strlen(name) >= MAX_NAME_LEN) return -ENAMETOOLONG;
    if (!strcmp(name, ".") || !strcmp(name, "..")) return -EINVAL;
    struct newfs_dentry_d entry = {0};
    strcpy(entry.name, name);
    /* 先复制名字，再截断字符串得到父路径。 */
    if (name == parent_path + 1) parent_path[1] = '\0';
    else name[-1] = '\0';
    int parent = lookup(parent_path);
    if (parent < 0) return parent;
    int ret = find_child(parent, entry.name);
    if (ret >= 0) return -EEXIST;
    if (ret != -ENOENT) return ret;
    struct newfs_inode_d *dir = &super.inodes[parent];
    if (dir->dir_count >= NEWFS_DIRECT_COUNT * NEWFS_DENTRY_PER_BLOCK) return -ENOSPC;
    int ino = alloc_bit(super.inode_map, NEWFS_INODE_COUNT);
    if (ino < 0) return ino;
    int added_block = dir->dir_count % NEWFS_DENTRY_PER_BLOCK == 0;
    if (added_block) {
        int block = alloc_bit(super.data_map, super.disk.data_blks);
        if (block < 0) {
            free_bit(super.inode_map, ino);
            return block;
        }
        dir->direct[dir->block_count++] = block;
    }
    entry.ino = ino;
    super.entries[parent][dir->dir_count] = entry;
    /* 新 inode 不分配数据块，父目录放不下新项时才扩容。 */
    super.inodes[ino] = (struct newfs_inode_d) {
        .ino = ino, .type = type, .atime = time(NULL), .mtime = time(NULL)
    };
    dir->dir_count++;
    dir->mtime = time(NULL);
    return 0;
}

int newfs_mkdir(const char *path, mode_t mode)
{
    (void)mode;
    return create_node(path, S_IFDIR);
}

int newfs_mknod(const char *path, mode_t mode, dev_t dev)
{
    (void)dev;
    if (!S_ISREG(mode)) return -EINVAL;
    return create_node(path, S_IFREG);
}

int newfs_getattr(const char *path, struct stat *st)
{
    int ino = lookup(path);
    if (ino < 0) return ino;
    struct newfs_inode_d *node = &super.inodes[ino];
    memset(st, 0, sizeof(*st));
    st->st_ino = ino;
    st->st_mode = node->type | NEWFS_DEFAULT_PERM;
    st->st_nlink = ino == 0 ? 2 : 1;
    st->st_uid = getuid();
    st->st_gid = getgid();
    st->st_size = node->dir_count * sizeof(struct newfs_dentry_d);
    st->st_blksize = NEWFS_BLOCK_SIZE;
    st->st_blocks = node->block_count * (NEWFS_BLOCK_SIZE / 512);
    st->st_atime = node->atime;
    st->st_mtime = st->st_ctime = node->mtime;
    return 0;
}

int newfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                  off_t offset, struct fuse_file_info *fi)
{
    (void)fi;
    if (offset < 0) return -EINVAL;
    int ino = lookup(path);
    if (ino < 0) return ino;
    struct newfs_inode_d *dir = &super.inodes[ino];
    if (dir->type != S_IFDIR) return -ENOTDIR;
    for (off_t i = offset; i < dir->dir_count; i++) {
        if (filler(buf, super.entries[ino][i].name, NULL, i + 1)) break;
    }
    return 0;
}

int newfs_utimens(const char *path, const struct timespec tv[2])
{
    int ino = lookup(path);
    if (ino < 0) return ino;
    int64_t now = time(NULL);
    if (!tv || tv[0].tv_nsec != UTIME_OMIT)
        super.inodes[ino].atime = !tv || tv[0].tv_nsec == UTIME_NOW ? now : tv[0].tv_sec;
    if (!tv || tv[1].tv_nsec != UTIME_OMIT)
        super.inodes[ino].mtime = !tv || tv[1].tv_nsec == UTIME_NOW ? now : tv[1].tv_sec;
    return 0;
}

void *newfs_init(struct fuse_conn_info *conn)
{
    (void)conn;
    int ret = newfs_mount(options.device);
    if (ret < 0) {
        fprintf(stderr, "挂载失败：%s\n", strerror(-ret));
        exit(EXIT_FAILURE);
    }
    return NULL;
}

void newfs_destroy(void *data)
{
    (void)data;
    int ret = newfs_umount();
    if (ret < 0) fprintf(stderr, "卸载写回失败：%s\n", strerror(-ret));
}

static struct fuse_operations operations = {
    .init = newfs_init, .destroy = newfs_destroy,
    .mkdir = newfs_mkdir, .mknod = newfs_mknod,
    .getattr = newfs_getattr, .readdir = newfs_readdir,
    .utimens = newfs_utimens
};

int main(int argc, char **argv)
{
    const struct fuse_opt spec[] = {
        {"--device=%s", offsetof(struct custom_options, device), 1},
        FUSE_OPT_END
    };
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    /* 不传 --device 时，驱动默认打开当前用户的 ~/ddriver。 */
    if (fuse_opt_parse(&args, &options, spec, NULL) < 0) return 1;
    /* 驱动共用 seek 位置，固定单线程避免并发请求相互干扰。 */
    if (fuse_opt_add_arg(&args, "-s") < 0) return 1;
    int ret = fuse_main(args.argc, args.argv, &operations, NULL);
    fuse_opt_free_args(&args);
    free(options.device);
    return ret;
}
