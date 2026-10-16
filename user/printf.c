#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#include <stdarg.h>

static char digits[] = "0123456789ABCDEF";

// 每次调用使用独立缓冲，短消息尽量一次写完，减少字符交错。
// 超过 128 字节就分段输出，不保证长消息整体连续。
struct printbuf {
  int fd;
  int used;
  char data[128];
};

static void flush(struct printbuf *out) {
  int done = 0;
  // write 可能只写出一部分，继续发送剩余内容。
  while (done < out->used) {
    int n = write(out->fd, out->data + done, out->used - done);
    if (n <= 0) break;
    done += n;
  }
  out->used = 0;
}

static void putc(struct printbuf *out, char c) {
  out->data[out->used++] = c;
  if (out->used == sizeof(out->data)) flush(out);
}

static void printint(struct printbuf *out, int xx, int base, int sgn) {
  char buf[16];
  int i, neg;
  uint x;

  neg = 0;
  if (sgn && xx < 0) {
    neg = 1;
    x = -xx;
  } else {
    x = xx;
  }

  i = 0;
  do {
    buf[i++] = digits[x % base];
  } while ((x /= base) != 0);
  if (neg) buf[i++] = '-';

  while (--i >= 0) putc(out, buf[i]);
}

static void printptr(struct printbuf *out, uint64 x) {
  int i;
  putc(out, '0');
  putc(out, 'x');
  for (i = 0; i < (sizeof(uint64) * 2); i++, x <<= 4) putc(out, digits[x >> (sizeof(uint64) * 8 - 4)]);
}

// Print to the given fd. Only understands %d, %x, %p, %s.
void vprintf(int fd, const char *fmt, va_list ap) {
  struct printbuf out = { .fd = fd, .used = 0 };
  char *s;
  int c, i, state;

  state = 0;
  for (i = 0; fmt[i]; i++) {
    c = fmt[i] & 0xff;
    if (state == 0) {
      if (c == '%') {
        state = '%';
      } else {
        putc(&out, c);
      }
    } else if (state == '%') {
      if (c == 'd') {
        printint(&out, va_arg(ap, int), 10, 1);
      } else if (c == 'l') {
        printint(&out, va_arg(ap, uint64), 10, 0);
      } else if (c == 'x') {
        printint(&out, va_arg(ap, int), 16, 0);
      } else if (c == 'p') {
        printptr(&out, va_arg(ap, uint64));
      } else if (c == 's') {
        s = va_arg(ap, char *);
        if (s == 0) s = "(null)";
        while (*s != 0) {
          putc(&out, *s);
          s++;
        }
      } else if (c == 'c') {
        putc(&out, va_arg(ap, uint));
      } else if (c == '%') {
        putc(&out, c);
      } else {
        // Unknown % sequence.  Print it to draw attention.
        putc(&out, '%');
        putc(&out, c);
      }
      state = 0;
    }
  }
  // 没有换行也要刷新，保证提示符能及时显示。
  flush(&out);
}

void fprintf(int fd, const char *fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  vprintf(fd, fmt, ap);
}

void printf(const char *fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  vprintf(1, fmt, ap);
}
