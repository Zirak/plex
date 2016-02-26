#define _XOPEN_SOURCE 600
#define _GNU_SOURCE
#include <pty.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

extern char **environ;

int create_master();
void father(int);
int spillout(int, int);
void child(int, char*, char*);

void alt(char *cmd) {
  int master_fd = create_master();
  if (master_fd < 0) {
    return;
  }

  char *pts = ptsname(master_fd);

  int slave_fd = open(pts, O_RDWR);
  pid_t pid = fork();

  if (pid > 0) {
    // Father
    close(slave_fd);
    father(master_fd);
  }
  else if (pid == 0) {
    // Child
    close(master_fd);
    child(slave_fd, pts, cmd);
  }
  else {
    perror("fork");
  }

  return;
}

int create_master() {
  int master_fd = posix_openpt(O_RDWR);
  if (master_fd < 0) {
    perror("posix_openpt");
    return -1;
  }

  printf("master = %d\n", master_fd);

  if (grantpt(master_fd) < 0) {
    perror("grantpt");
    return -1;
  }

  if (unlockpt(master_fd) < 0) {
    perror("unlockpt");
    return -1;
  }

  printf("cpath = %s\n", ptsname(master_fd));
  return master_fd;
}

void father(int master_fd) {
  int stdin_fd = fileno(stdin);

  fd_set inps;

  while (1) {
    FD_ZERO(&inps);
    FD_SET(fileno(stdin), &inps);
    FD_SET(master_fd, &inps);

    if (select(master_fd+1, &inps, NULL, NULL, NULL) < 0) {
      perror("select");
      return;
    }

    if (FD_ISSET(stdin_fd, &inps)) {
      // stdin -> slave

      if (spillout(stdin_fd, master_fd) < 0) {
        // TODO kill child?
        perror("stdin -> slave");
        return;
      }
    }

    if (FD_ISSET(master_fd, &inps)) {
      // slave -> stdin

      // TODO kill child?
      if (spillout(master_fd, stdin_fd) < 0) {
        perror("slave -> stdin");
        return;
      }
    }
  }
}

int spillout(int src_fd, int dst_fd) {
  char input[512] = { 0 };
  size_t input_size = 512;

  int read_count = read(src_fd, input, input_size);

  if (read_count > 0) {
    int write_count = write(dst_fd, input, read_count);

    if (write_count < 0) {
      return -1;
    }
  }
  else {
    return -1;
  }

  return 0;
}

void child(int slave_fd, char *pts, char *cmd) {
  struct termios slave_settings;

  tcgetattr(slave_fd, &slave_settings);
  cfmakeraw(&slave_settings);
  tcsetattr(slave_fd, TCSANOW, &slave_settings);

  dup2(slave_fd, 0);
  dup2(slave_fd, 1);
  dup2(slave_fd, 2);

  if (setsid() < 0) {
    perror("setsid");
    exit(EXIT_FAILURE);
  }

  if (ioctl(slave_fd, TIOCSCTTY, 1)) {
    perror("ioctl TIOCSCTTY");
    exit(EXIT_FAILURE);
  }

  // TODO sigchild handler

  char *args[] = { cmd, "-h", NULL };

  // this is where the magic happens
  // XXX think about the specifics
  setenv("USEROUT_PATH", pts, 1);
  setenv("USERIN_PATH", pts, 1);

  execve(cmd, args, environ);

  // we only get here on errors
  perror("failed to create child");
  exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
  char *cmd;

  if (argc < 2) {
    cmd = "/bin/sh";
  }
  else {
    cmd = argv[1];
  }

  alt(cmd);

  return 0;
}
