#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <err.h>
#include <test161/test161.h>

#define _PATH_MYSELF "bin/cp"


int
main(int argc, char *argv[])
{
 (void)argc;
 (void) argv;
  int child_pid;
  int p_pid;

  // argv[0] = "cp";
  // argv[1] = "file1";
  // argv[2] = "file2";
  // argv[3] = NULL;


  warnx("Starting ...\n");
  int pid = fork();



  if (pid == 0) {
	child_pid = getpid();
	tprintf("I'm the child, my pid = %d\n", child_pid);
	execv(_PATH_MYSELF, argv);
	exit(0);
  } else {
    int ret;
	p_pid = getpid();
    tprintf("I'm the parent,mypid = %d---\n", p_pid);
    waitpid(pid, &ret, 0);
	exit(1);
  }

  return 0;
}
