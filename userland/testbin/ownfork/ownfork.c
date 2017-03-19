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
  int child_pid;
  int p_pid;

  warnx("Starting ...\n");
  int pid = fork();

  if (pid == 0) {
	  child_pid = getpid();
    tprintf("~~~~~~~&&&&&~~im the child,mypid = %d~~~~~~~~~~~~\n", child_pid);

  } else {
    // int ret;
	p_pid = getpid();
    tprintf("~~~~~~~&&&~~~~~~~Im the parent,mypid = %d---\n", p_pid);
    // waitpid(pid, &ret, 0);

  }
  return 0;
}
