#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  // argc 包含程序名，一个时间参数对应 argc == 2。
  if(argc != 2){
    printf("Sleep needs one argument!\n");
    exit(-1);
  }

  // 参数单位是时钟滴答，先把字符串转成整数。
  int ticks = atoi(argv[1]);
  // sleep 是系统调用，等待期间会让出 CPU。
  sleep(ticks);
  printf("Sleep %d\n", ticks);
  // 用 exit 通知内核结束当前进程。
  exit(0);
}