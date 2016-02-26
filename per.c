#include <stdio.h>

#include "better-streams.h"

int main(void) {
  FILE *userout = getuserout();

  fprintf(stdout, "blah\n");
  fprintf(userout, "hurray!\n");

  char *line = NULL;
  size_t line_len = 0;

  fprintf(userout, "Reading from stdin:\n");
  getline(&line, &line_len, stdin);
  fprintf(userout, "From stdin: %s", line);

  fprintf(userout, "Reading from userin:\n");
  getline(&line, &line_len, getuserin());
  fprintf(userout, "From userin: %s", line);

  return 0;
}
