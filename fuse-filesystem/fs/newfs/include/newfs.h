#ifndef NEWFS_H
#define NEWFS_H

#define FUSE_USE_VERSION 26
#include <fuse.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "ddriver.h"
#include "types.h"

#define NEWFS_MAGIC 0x4e465335
#define NEWFS_DEFAULT_PERM 0777

int newfs_mount(const char *device);
int newfs_umount(void);
void *newfs_init(struct fuse_conn_info *conn);
void newfs_destroy(void *data);
int newfs_mkdir(const char *path, mode_t mode);
int newfs_mknod(const char *path, mode_t mode, dev_t dev);
int newfs_getattr(const char *path, struct stat *st);
int newfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                  off_t offset, struct fuse_file_info *fi);
int newfs_utimens(const char *path, const struct timespec tv[2]);

#endif
