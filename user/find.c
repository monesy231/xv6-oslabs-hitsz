#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

// 在 path 目录下递归查找 name
void
find(char *path, char *name)
{
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  // 打开当前路径
  if ((fd = open(path, 0)) < 0) {
    fprintf(2, "find: cannot open %s\n", path);
    return;
  }

  // 获取当前路径的信息
  if (fstat(fd, &st) < 0) {
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return;
  }

  switch (st.type) {

  case T_FILE:
    // 如果传进来的 path 本身是普通文件，这里不需要继续递归
    break;

  case T_DIR:
    // 防止拼接后的路径超过 buf 大小
    if (strlen(path) + 1 + DIRSIZ + 1 > sizeof(buf)) {
      printf("find: path too long\n");
      break;
    }

    // 复制当前路径
    strcpy(buf, path);

    // p 指向路径末尾
    p = buf + strlen(buf);

    // 后面准备拼接 "/文件名"
    *p++ = '/';

    // 不断读取目录项
    while (read(fd, &de, sizeof(de)) == sizeof(de)) {

      // 无效目录项
      if (de.inum == 0)
        continue;

      // 把目录项名字拼到路径后面
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;

      // 一定要跳过 . 和 ..
      if (strcmp(p, ".") == 0 || strcmp(p, "..") == 0)
        continue;

      // 如果名字匹配，打印完整路径
      if (strcmp(p, name) == 0) {
        printf("%s\n", buf);
      }

      // 获取这个目录项的信息
      if (stat(buf, &st) < 0) {
        printf("find: cannot stat %s\n", buf);
        continue;
      }

      // 如果它还是一个目录，就递归进去
      if (st.type == T_DIR) {
        find(buf, name);
      }
    }

    break;
  }

  close(fd);
}

int
main(int argc, char *argv[])
{
  if (argc != 3) {
    fprintf(2, "usage: find <path> <name>\n");
    exit(1);
  }

  find(argv[1], argv[2]);

  exit(0);
}