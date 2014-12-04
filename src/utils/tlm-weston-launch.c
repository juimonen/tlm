#include "config.h"

#include <error.h>
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#define MAX_ARGV_SIZE 128


void start_weston_launcher(int argc, char *argv[]) {
  char *child_argv[MAX_ARGV_SIZE] = {0, 0, };
  int i = 0, j = 0;

  child_argv[i++] = "/bin/sh";
  child_argv[i++] = "-c";
  child_argv[i++] = BINDIR "/weston-launch";

  if (i + argc >= MAX_ARGV_SIZE) argc -= i;

  for (j = 0; j < argc; ++j)
    child_argv[i + j] = argv[j];
  child_argv[i + j] = NULL;

  execv(child_argv[0], child_argv);
  
  fprintf(stderr, "exec failed: %s", strerror(errno));
}

static void help() {
  fprintf(stdout, "tlm-weston-launch - Starts weston-launch in background");
}

int main(int argc, char *argv[]) {
  struct option opts[] = {
    { "help",    no_argument,       NULL, 'h' },
    { 0,         0,                 NULL,  0  }
  };
  pid_t child_pid = 0;
  int i, c;

  while ((c = getopt_long(argc, argv, "h", opts, &i)) != -1) {
    switch(c) {
      case 'h':
        help();
        return 0;
    }
  }

  if ((child_pid = fork()) < 0) {
    fprintf(stderr, "fork() failed: %s", strerror(errno));
    return -1;
  }

  if (child_pid == 0) {
    start_weston_launcher(argc, argv);
    return 0;
  }

  return 0;
}
