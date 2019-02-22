#ifdef __LINUX__
#include "PlatformLinux.h"


static void segvHandler(int sig) {
  void *array[99];
  size_t size;

  // get void*'s for all entries on the stack
  size = backtrace(array, 99);

  // print out all the frames to stderr
  fprintf(stderr, "Error: signal %d:\n", sig);
  backtrace_symbols_fd(array, size, STDERR_FILENO);
  exit(1);
}


void PlatformInit()
{
  signal(SIGSEGV, segvHandler);
  signal(SIGABRT, segvHandler);
  signal(SIGTRAP, segvHandler);
}

void PlatformOpenUrl(const char * url)
{


  int fd;

  if (fork() != 0) return;

  /* 关闭由iptux打开的所有可能的文件描述符 */
  fd = 3;
  while (fd < FD_SETSIZE) {
    close(fd);
    fd++;
  }
  /* 脱离终端控制 */
  setsid();

  /* 打开URL */
  execlp("xdg-open", "xdg-open", url, NULL);
  /* 测试系统中所有可能被安装的浏览器 */
  execlp("firefox", "firefox", url, NULL);
  execlp("opera", "opera", url, NULL);
  execlp("konqueror", "konqueror", url, NULL);
  execlp("open", "open", url, NULL);
  pwarning(_("Can't find any available web browser!\n"));
}

#endif
