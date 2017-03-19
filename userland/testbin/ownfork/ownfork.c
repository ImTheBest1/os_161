#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <err.h>
#include <test161/test161.h>

int
main(int argc, char *argv[])
{
  (void)argc; // dont need these
  (void)argv; // dont need these

  warnx("Starting ...\n");
  int pid = fork();

  if (pid == 0) {
    tprintf("~~~~~~~&&&&&~~im child~~~~~~~~~~~~\n");
    exit(1);
  } else {
    int ret;
    tprintf("~~~~~~~&&&~~~~~~~~~~~parent---\n");
    waitpid(pid, &ret, 0);
    exit(0);
  }
  return 0;
}
