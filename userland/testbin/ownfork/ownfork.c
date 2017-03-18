#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <err.h>
#include <test161/test161.h>


int
main(int argc, char *argv[])
{
  (void)argc; // dont need these
  (void)argv; // dont need these

  warnx("Starting ...\n");
  int pid1 = fork();

  if (pid1 == 0) {
	  tprintf(" fork is created \n");
     exit(1);
  } else {
    int ret;
    waitpid(pid1, &ret, 0);
    // exit(0);
}

  return 0;
}
