#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <misc.h>
#include <stringsplus.h>
#include <sys/types.h>
#include <unistd.h>

void usage(char *prog) {
  printf("\n\
PROGRAM:      %s\n\
DESCRIPTION:  Randomly choose k lines from a file of n lines, for 0 < k < n.\n\
USAGE:        %s [OPTIONS] <infile>\n\
OPTIONS:\n\
    -k <k>    Number of lines to choose (default is all lines).\n\
    -r        Randomize order (not implemented).\n\
    -h        Print this help message.\n\n", prog, prog);
  exit(0);
}

int main(int argc, char *argv[]) {
  FILE *INF, *F;
  int i, n, k = -1, randomize = 0;
  String *line = str_new(STR_MED_LEN);
  int *chosen;
  char c;
  while ((c = getopt(argc, argv, "k:rh")) != -1) {
    switch (c) {
    case 'k':
      k = atoi(optarg);
      if (k <= 0) die("ERROR: k must be greater than 0.\n");
      break;
    case 'r':
      randomize = 1;
      break;
    case 'h':
      usage(argv[0]);
    case '?':
      die("Bad argument.  Try '%s -h'.\n", argv[0]);
    }
  }

  if (optind != argc - 1) 
    die("Input filename required.  Try '%s -h'.\n", argv[0]);

  INF = fopen_fname(argv[optind], "r");

  /* scan for number of lines */
  if (!strcmp(argv[optind], "-")) { /* if stdin, need temp file */
    char tmpstr[20];
    sprintf(tmpstr, "choose_lines.%d", getpid());
    F = fopen_fname(tmpstr, "w+");
    for (n = 0; str_readline(line, INF) != EOF; n++) 
      fprintf(F, line->chars);
    fclose(F);
    INF = fopen_fname(tmpstr, "r");
  }
  else 
    for (n = 0; str_readline(line, INF) != EOF; n++);

  if (k == -1) k = n;

  chosen = smalloc(n * sizeof(int));
  choose(chosen, n, k);

  rewind(INF);
  for (i = 0; str_readline(line, INF) != EOF; i++) 
    if (chosen[i]) printf(line->chars);
  
  return 0;
}