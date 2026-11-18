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

  // 只读打开当前路径，失败时结束本层查找。
  if ((fd = open(path, 0)) < 0) {
    fprintf(2, "find: cannot open %s\n", path);
    return;
  }

  // fstat 判断当前路径是文件还是目录。
  if (fstat(fd, &st) < 0) {
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return;
  }

  switch (st.type) {

  case T_FILE:
    // 普通文件不再向下遍历。
    break;

  case T_DIR:
    // 为斜杠、目录项名字和字符串结束符留够空间。
    if (strlen(path) + 1 + DIRSIZ + 1 > sizeof(buf)) {
      printf("find: path too long\n");
      break;
    }

    // 拼接子项路径，p 指向文件名的位置。
    strcpy(buf, path);

    p = buf + strlen(buf);

    *p++ = '/';

    // 每次读取一个完整目录项，读不到时结束遍历。
    while (read(fd, &de, sizeof(de)) == sizeof(de)) {

      // inum 为 0 表示空目录项。
      if (de.inum == 0)
        continue;

      // 目录项名字可能没有结束符，复制后手动补 0。
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;

      // 跳过自身和父目录，避免无限递归。
      if (strcmp(p, ".") == 0 || strcmp(p, "..") == 0)
        continue;

      // 文件和目录都匹配；同名目录只在这里输出一次。
      if (strcmp(p, name) == 0) {
        printf("%s\n", buf);
      }

      if (stat(buf, &st) < 0) {
        printf("find: cannot stat %s\n", buf);
        continue;
      }

      // 递归只检查子项，不重复输出当前目录。
      if (st.type == T_DIR) {
        find(buf, name);
      }
    }

    break;
  }

  // 关闭本层打开的描述符，避免递归时耗尽资源。
  close(fd);
}

int
main(int argc, char *argv[])
{
  // argv[1] 是起始目录，argv[2] 是要查找的名字。
  if (argc != 3) {
    fprintf(2, "usage: find <path> <name>\n");
    exit(1);
  }

  find(argv[1], argv[2]);

  exit(0);
}