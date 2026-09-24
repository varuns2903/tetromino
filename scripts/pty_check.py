#!/usr/bin/env python3
"""Headless terminal-restoration checks for tetromino.

Runs the binary on a pseudo-terminal, drives it with keystrokes / signals and
verifies that afterwards the tty's termios settings are byte-for-byte what
they were before, and that the output ends with the restore sequences
(show cursor, leave alternate screen).

Usage: scripts/pty_check.py [path/to/tetromino]
"""

import fcntl
import os
import pty
import select
import signal
import struct
import sys
import termios
import time

BINARY = sys.argv[1] if len(sys.argv) > 1 else "build/tetromino"

SHOW_CURSOR = b"\x1b[?25h"
LEAVE_ALT = b"\x1b[?1049l"


def set_winsize(fd, rows, cols):
    fcntl.ioctl(fd, termios.TIOCSWINSZ, struct.pack("HHHH", rows, cols, 0, 0))


def drain(fd, seconds):
    out = b""
    deadline = time.time() + seconds
    while time.time() < deadline:
        r, _, _ = select.select([fd], [], [], max(0.0, deadline - time.time()))
        if not r:
            break
        try:
            chunk = os.read(fd, 65536)
        except OSError:
            break
        if not chunk:
            break
        out += chunk
    return out


def run_case(name, actions, expect_signal=None, args=()):
    master, slave = pty.openpty()
    set_winsize(slave, 30, 90)
    before = termios.tcgetattr(slave)

    pid = os.fork()
    if pid == 0:
        os.setsid()
        fcntl.ioctl(slave, termios.TIOCSCTTY, 0)
        for fd in (0, 1, 2):
            os.dup2(slave, fd)
        os.close(master)
        os.close(slave)
        os.execv(BINARY, [BINARY, *args])

    output = drain(master, 0.5)
    for act in actions:
        kind, value = act
        if kind == "keys":
            os.write(master, value)
        elif kind == "signal":
            os.kill(pid, value)
        elif kind == "resize":
            set_winsize(slave, *value)
            os.kill(pid, signal.SIGWINCH)  # the kernel also sends it to the fg pgrp
        elif kind == "sleep":
            time.sleep(value)
        output += drain(master, 0.2)

    # Wait for exit (with timeout).
    status = None
    for _ in range(50):
        wpid, st = os.waitpid(pid, os.WNOHANG)
        if wpid == pid:
            status = st
            break
        output += drain(master, 0.1)
    if status is None:
        os.kill(pid, signal.SIGKILL)
        os.waitpid(pid, 0)
        print(f"FAIL {name}: process did not exit")
        return False, output

    output += drain(master, 0.1)
    after = termios.tcgetattr(slave)
    os.close(master)
    os.close(slave)

    ok = True
    problems = []
    if before != after:
        ok = False
        problems.append("termios differs after exit")
    tail = output[-200:]
    if SHOW_CURSOR not in tail or LEAVE_ALT not in tail:
        ok = False
        problems.append(f"restore sequences missing from output tail: {tail!r}")
    if expect_signal is not None:
        if not os.WIFSIGNALED(status) or os.WTERMSIG(status) != expect_signal:
            ok = False
            problems.append(f"expected death by signal {expect_signal}, got status {status}")
    else:
        if not os.WIFEXITED(status) or os.WEXITSTATUS(status) != 0:
            ok = False
            problems.append(f"expected exit 0, got status {status}")

    print(("PASS " if ok else "FAIL ") + name + ("" if ok else ": " + "; ".join(problems)))
    return ok, output


CASES = [
    ("quit with q", [("keys", b"q")], None),
    ("quit with Escape from start screen (lone ESC after timeout)", [("keys", b"\x1b")], None),
    ("quit with Ctrl-C byte", [("keys", b"\x03")], None),
    ("arrow keys then q", [("keys", b"\x1b[A\x1b[B\x1bOC\x1b[D"), ("keys", b"q")], None),
    ("resize then q", [("resize", (40, 120)), ("resize", (20, 50)), ("keys", b"q")], None),
    ("SIGTERM", [("signal", signal.SIGTERM)], None),
    ("SIGHUP", [("signal", signal.SIGHUP)], None),
    ("SIGSEGV (crash path)", [("signal", signal.SIGSEGV)], signal.SIGSEGV),
    ("SIGABRT (crash path)", [("signal", signal.SIGABRT)], signal.SIGABRT),
]


def main():
    extra = [a for a in os.environ.get("PTY_CHECK_ARGS", "").split() if a]
    results = [run_case(n, a, s, extra)[0] for n, a, s in CASES]
    print(f"{sum(results)}/{len(results)} passed")
    sys.exit(0 if all(results) else 1)


if __name__ == "__main__":
    main()
