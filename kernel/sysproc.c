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

// Report an observation of the next runnable process, then yield normally.
uint64 sys_yield(void) {
  struct proc *p = myproc();
  int next_pid = p->pid;
  uint64 next_pc = p->trapframe->epc;
  int current = p - proc;

  // Keep the single-CPU observation stable while scanning and printing.
  // On multiple CPUs, another scheduler can still run the candidate.
  push_off();
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

  // If nobody else is runnable, yielding will make p eligible again.
  printf("Save the context of the process to the memory region from address %p to %p\n",
         (uint64)&p->context, (uint64)(&p->context + 1));
  printf("Current running process pid is %d and user pc is %p\n",
         p->pid, p->trapframe->epc);
  printf("Next runnable process pid is %d and user pc is %p\n", next_pid, next_pc);
  pop_off();

  // sched() must see only the lock acquired inside yield().
  yield();
  return 0;
}
