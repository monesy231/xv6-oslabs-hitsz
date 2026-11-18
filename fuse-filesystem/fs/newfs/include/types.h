#ifndef NEWFS_TYPES_H
#define NEWFS_TYPES_H

#include <stdint.h>

#define MAX_NAME_LEN 128
#define NEWFS_BLOCK_SIZE 1024
#define NEWFS_INODE_COUNT 512
#define NEWFS_DIRECT_COUNT 6
#define NEWFS_INODE_BLOCKS 32
#define NEWFS_DATA_START (3 + NEWFS_INODE_BLOCKS)

struct custom_options {
    char *device;
};

/* 磁盘超级块：offset 是字节偏移，blks 是逻辑块数。 */
struct newfs_super_d {
    uint32_t magic;
    uint32_t blk_size;
    uint32_t sb_offset, sb_blks;
    uint32_t ino_map_offset, ino_map_blks;
    uint32_t data_map_offset, data_map_blks;
    uint32_t ino_offset, ino_blks;
    uint32_t data_offset, data_blks;
    uint32_t root_ino, max_inodes;
};

/* 一个 inode 固定 64 字节，一个逻辑块放 16 个。 */
struct newfs_inode_d {
    uint32_t ino;
    uint32_t type;                      /* S_IFDIR 或 S_IFREG */
    uint32_t dir_count;                 /* 目录中的子项数 */
    uint32_t block_count;               /* 已分配的数据块数 */
    uint32_t direct[NEWFS_DIRECT_COUNT]; /* 数据区内的相对块号 */
    int64_t atime, mtime;
    uint32_t reserved[2];
};

/* 父目录的数据块保存子项的名字和 inode 编号。 */
struct newfs_dentry_d {
    char name[MAX_NAME_LEN];
    uint32_t ino;
};

#define NEWFS_DENTRY_PER_BLOCK (NEWFS_BLOCK_SIZE / sizeof(struct newfs_dentry_d))

/* 挂载时读入目录项，运行中直接使用缓存，卸载时统一写回。 */
struct newfs_super {
    int fd;
    int io_size, disk_size;
    struct newfs_super_d disk;
    uint8_t inode_map[NEWFS_BLOCK_SIZE];
    uint8_t data_map[NEWFS_BLOCK_SIZE];
    struct newfs_inode_d inodes[NEWFS_INODE_COUNT];
    struct newfs_dentry_d entries[NEWFS_INODE_COUNT]
                                [NEWFS_DIRECT_COUNT * NEWFS_DENTRY_PER_BLOCK];
};

#endif
