#include <stdio.h>
#include <stdlib.h>

FILE *getuserout() {
  char *userout_path = getenv("USEROUT_PATH");

  if (userout_path == NULL) {
    // XXX
    return stdout;
  }

  return fopen(userout_path, "wb");
}

FILE *getuserin() {
  char *userin_path = getenv("USERIN_PATH");

  if (userin_path == NULL) {
    // XXX
    return stdin;
  }

  return fopen(userin_path, "rb");
}
