#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

extern struct proc proc[NPROC];

uint64 sys_exit(void) {
  int n;
  if (argint(0, &n) < 0) return -1;
  exit(n);
  return 0;  // not reached
}

uint64 sys_getpid(void) { return myproc()->pid; }

uint64 sys_fork(void) { return fork(); }

uint64 sys_wait(void) {
  uint64 p;
  int flags;
  // 从保存的 a0、a1 取出状态地址和 flags，交给内核 wait。
  if (argaddr(0, &p) < 0 || argint(1, &flags) < 0) return -1;
  return wait(p, flags);
}

uint64 sys_sbrk(void) {
  int addr;
  int n;

  if (argint(0, &n) < 0) return -1;
  addr = myproc()->sz;
  if (growproc(n) < 0) return -1;
  return addr;
}

uint64 sys_sleep(void) {
  int n;
  uint ticks0;

  if (argint(0, &n) < 0) return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while (ticks - ticks0 < n) {
    if (myproc()->killed) {
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64 sys_kill(void) {
  int pid;

  if (argint(0, &pid) < 0) return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64 sys_uptime(void) {
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64 sys_rename(void) {
  char name[16];
  int len = argstr(0, name, MAXPATH);
  if (len < 0) {
    return -1;
  }
  struct proc *p = myproc();
  memmove(p->name, name, len);
  p->name[len] = '\0';
  return 0;
}

// 先打印调度信息，再调用内核 yield 真正让出 CPU。
uint64 sys_yield(void) {
  struct proc *p = myproc();
  // 没有其他就绪进程时，当前进程仍可再次被调度。
  int next_pid = p->pid;
  uint64 next_pc = p->trapframe->epc;
  int current = p - proc;

  // 关本核中断，保证单核观察稳定；其他核仍可能调度候选进程。
  push_off();
  // 从当前槽位之后环形查找，在锁内读取就绪进程的信息。
  for (int offset = 1; offset < NPROC; offset++) {
    struct proc *next = &proc[(current + offset) % NPROC];
    acquire(&next->lock);
    if (next->state == RUNNABLE) {
      next_pid = next->pid;
      next_pc = next->trapframe->epc;
      release(&next->lock);
      break;
    }
    release(&next->lock);
  }

  // context 区间左闭右开，共 112 字节；指针加 1 跨过整个结构体。
  printf("Save the context of the process to the memory region from address %p to %p\n",
         (uint64)&p->context, (uint64)(&p->context + 1));
  // 用户态 PC 取 trapframe->epc，不是内核 context 中的 ra。
  printf("Current running process pid is %d and user pc is %p\n",
         p->pid, p->trapframe->epc);
  printf("Next runnable process pid is %d and user pc is %p\n", next_pid, next_pc);
  // 先平衡额外的 push_off，再让 yield 持进程锁进入 sched。
  pop_off();

  yield();
  return 0;
}
