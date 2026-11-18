#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    // 管道只能单向传输，往返通信需要两条。
    int p2c[2];   // 父写子读：0 是读端，1 是写端
    int c2p[2];   // 子写父读

    pipe(p2c);
    pipe(c2p);

    // 先建管道再 fork，父子进程才能使用同一组管道。
    int pid = fork();

    if(pid < 0){
        fprintf(2, "fork failed\n");
        exit(1);
    }

    // fork 在子进程中返回 0，在父进程中返回子 PID。
    if(pid == 0){
        // 子进程读 p2c、写 c2p，其余端点关闭。
        close(p2c[1]);
        close(c2p[0]);

        int parent_pid;

        // 按一个 int 的长度读取父 PID，再打印 ping。
        read(p2c[0], &parent_pid, sizeof(parent_pid));

        printf("%d: received ping from pid %d\n",
               getpid(), parent_pid);

        // 把自己的 PID 作为回复内容。
        int child_pid = getpid();
        write(c2p[1], &child_pid, sizeof(child_pid));

        close(p2c[0]);
        close(c2p[1]);

        exit(0);
    } else {
        // 父进程写 p2c、读 c2p，其余端点关闭。
        close(p2c[0]);
        close(c2p[1]);

        int parent_pid = getpid();

        // PID 通过管道传递，不能假定父子 PID 相邻。
        write(p2c[1], &parent_pid, sizeof(parent_pid));

        close(p2c[1]);

        int child_pid;

        // 等子进程回复后再打印 pong，保证输出顺序。
        read(c2p[0], &child_pid, sizeof(child_pid));

        printf("%d: received pong from pid %d\n",
               getpid(), child_pid);

        close(c2p[0]);

        // 回收子进程，避免留下僵尸进程。
        wait(0);
        exit(0);
    }
}