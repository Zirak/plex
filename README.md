A POC of (some of) the ideas presented in [my rant](https://medium.com/@zirakertan/rant-the-shell-terminal-4f45bb29dac8).

* `plex.c` is a POC terminal emulator which supports the `userin` and `userout` streams described in the rant.
* `per.c` is a stupid program which uses said `userin` and `userout`.
* `getuserin` and `getuserout` are defined in `better-streams.h`. Just include and you can enjoy the wonders.

# Previewing

```sh
$ make
$ ./plex /bin/bash
echo 4 | ./per &> /dev/null
```

# External patches
Here's a bunch of patches to make `userin` and `userout` work in actual TEs.

## ssh
Done against [72b061d4ba0f909501c595d709ea76e06b01e5c9](https://github.com/openssh/openssh-portable/tree/72b061d4ba0f909501c595d709ea76e06b01e5c9).

```patch
diff --git a/session.c b/session.c
index 7a02500..639f1e4 100644
--- a/session.c
+++ b/session.c
@@ -1249,8 +1249,11 @@ do_setup_env(Session *s, const char *shell)
        free(laddr);
        child_set_env(&env, &envsize, "SSH_CONNECTION", buf);

-       if (s->ttyfd != -1)
+       if (s->ttyfd != -1) {
                child_set_env(&env, &envsize, "SSH_TTY", s->tty);
+               child_set_env(&env, &envsize, "USERIN_PATH", s->tty);
+               child_set_env(&env, &envsize, "USEROUT_PATH", s->tty);
+       }
        if (s->term)
                child_set_env(&env, &envsize, "TERM", s->term);
        if (s->display)
```

## tmux
Done against [e9d369a09e48ea8f940958025c8444988d31e840](https://github.com/tmux/tmux/tree/e9d369a09e48ea8f940958025c8444988d31e840).

```patch
diff --git a/window.c b/window.c
index a364948..04f50f5 100644
--- a/window.c
+++ b/window.c
@@ -875,6 +875,10 @@ window_pane_spawn(struct window_pane *wp, int argc, char **argv,

 		if (path != NULL)
 			environ_set(env, "PATH", "%s", path);
+		// zirak: better-streams
+		environ_set(env, "USERIN_PATH", "%s", wp->tty);
+		environ_set(env, "USEROUT_PATH", "%s", wp->tty);
+		// /zirak
 		environ_set(env, "TMUX_PANE", "%%%u", wp->id);
 		environ_push(env);
```

# Roadmap
In this section I'll pour the solutions I've explored and other random thoughts.

## Defining userin and userout
How can `userin` and `userout` be defined?

### Create our own `/dev/tty`
`/dev/tty` is a magical being which talks to the terminal. If we can re-create it then we win.

That's not very possible (or at least probable). `/dev/tty` is its own inode type, creating it requires `mknod`, and from there it's all syscalls.

While reading into it I discovered that a tty is a [kernelic object](http://lxr.free-electrons.com/source/include/uapi/asm-generic/termbits.h#L11) with its [own set of ioctls](http://man7.org/linux/man-pages/man4/tty_ioctl.4.html)!

### Write a kernel module
Yeah, no. That'd be modifying a kernel struct, which means that to use `userout` your *users* will have to run a kernel module or wait for it to be included in the kernel itself (...yeah).

### Then look at isatty
How does `isatty` work? Does it somehow talk to your terminal with awesome black magic?

The answer is no. It's just an `ioctl` (`tcgetattr`).

### Inject the fds to your children
Difficult, error prone, and non-recursive. They'd have to also be inejcted into your children and grand-children.

Not very applicable (even if it's cool).

### Find PID of terminal
And communicate via pipes? Maybe.

### Environment variables
Simplest and most backwards-compliant way.

```sh
USEROUT_PATH=/path/to/userout
USERIN_PATH=/path/to/userin
```

Every program in the universe can read from env so it can be implemented without libc.

The downside is that this will forever and ever be the way. If userin will one day be part of the kernelic termios struct we'll still have to support this way.

### What about Windows?
Apparently, these are (sort of) built into Windows:

- [WriteConsoleOutput](https://msdn.microsoft.com/en-us/library/windows/desktop/ms687404.aspx)
- [ReadConsoleInput](https://msdn.microsoft.com/en-us/library/windows/desktop/ms684961.aspx)

Except they're hidden behind WinAPI instead of being regular `FILE`s.

POSIX: 0. WinAPI: 1. Who knew.

I haven't planned if and how support will be added.

## forkstream
I'm still in the black on this one, especially because its name is so
bad. Thinking of different things.

Here's the simplest case:

```c
FILE *image_out = forkstream("image/tiff");
fwrite("\49\49\2a\00", 4, 1, image_out);
// ...

FILE *another_img = forkstream("image/png");
fwrite("\89PNG\0d\0a\1a\0a", 8, 1, another_img);
// ...
```

In case userout isn't defined (= the terminal doesn't support this kind of
behaviour) it's easy to simply write to stdout.

Things get weirder in multi-threaded (and multi-process) environments:

```c
switch (fork()) {
case -1:
  perror("fork()");
  break;

case 0: {
  FILE *image_out = forkstream("image/tiff");
  fwrite("\49\49\2a\00", 4, 1, image_out);
  // ...
  break;
}

default: {
  FILE *another_img = forkstream("image/png");
  fwrite("\89PNG\0d\0a\1a\0a", 8, 1, another_img);
  // ...
}
}
```

On compliant terminals this makes sense: Two streams are created, each with its
own allocated space, and the images are drawn to their respective places.

But what about non-compliant terminals? If it just pours into stdout then
they'll display data that should be serial (first the tiff, then the png) in a
garbled way (some of the tiff then some of the png then some of the tiff, ...).

Delaying and synchronising writes? Maybe this shouldn't be a problem?

Maybe forkstream should return `NULL` on non-compliant terminals?

* License
WTFPL.
