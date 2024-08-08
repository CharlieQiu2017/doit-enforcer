#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>

static inline long syscall (long num, long arg1, long arg2, long arg3, long arg4, long arg5, long arg6) {
  long ret;
  register long _num __asm__ ("rax") = num;
  register long _arg1 __asm__ ("rdi") = arg1;
  register long _arg2 __asm__ ("rsi") = arg2;
  register long _arg3 __asm__ ("rdx") = arg3;
  register long _arg4 __asm__ ("r10") = arg4;
  register long _arg5 __asm__ ("r8") = arg5;
  register long _arg6 __asm__ ("r9") = arg6;
  __asm__  volatile (
    "syscall\n"
  : "=a"(ret)
  : "r"(_arg1), "r"(_arg2), "r"(_arg3), "r"(_arg4), "r"(_arg5), "r"(_arg6), "0"(_num)
  : "rcx", "r11", "memory", "cc"
  );
  return ret;
}

#define SYS_READ 0
#define SYS_WRITE 1
#define SYS_EXIT 60

static inline void exit (long status) {
  syscall (SYS_EXIT, status, 0, 0, 0, 0, 0);
}

static inline void read (int fd, void* buf, long count) {
  syscall (SYS_READ, fd, (long) buf, count, 0, 0, 0);
}

static inline void write (int fd, void* buf, long count) {
  syscall (SYS_WRITE, fd, (long) buf, count, 0, 0, 0);
}

#endif
