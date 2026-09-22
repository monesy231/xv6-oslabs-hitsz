#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    int p2c[2];   // parent -> child
    int c2p[2];   // child -> parent

    pipe(p2c);
    pipe(c2p);

    int pid = fork();

    if(pid < 0){
        fprintf(2, "fork failed\n");
        exit(1);
    }

    if(pid == 0){
        // 子进程

        close(p2c[1]);   // 子进程不用父->子的写端
        close(c2p[0]);   // 子进程不用子->父的读端

        int parent_pid;

        // 接收父进程发来的 PID
        read(p2c[0], &parent_pid, sizeof(parent_pid));

        printf("%d: received ping from pid %d\n",
               getpid(), parent_pid);

        // 把自己的 PID 发回父进程
        int child_pid = getpid();
        write(c2p[1], &child_pid, sizeof(child_pid));

        close(p2c[0]);
        close(c2p[1]);

        exit(0);
    } else {
        // 父进程

        close(p2c[0]);   // 父进程不用父->子的读端
        close(c2p[1]);   // 父进程不用子->父的写端

        int parent_pid = getpid();

        // 把自己的 PID 发给子进程
        write(p2c[1], &parent_pid, sizeof(parent_pid));

        close(p2c[1]);

        int child_pid;

        // 接收子进程发回来的 PID
        read(c2p[0], &child_pid, sizeof(child_pid));

        printf("%d: received pong from pid %d\n",
               getpid(), child_pid);

        close(c2p[0]);

        wait(0);
        exit(0);
    }
}