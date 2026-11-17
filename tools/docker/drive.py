#!/usr/bin/env python3
"""Drive kyu-update in a real terminal, as a person would.

Runs the command in a pseudo-terminal, answers what it asks (sudo's
password, pacman's and Kyu's questions, the last key), and records:
  transcript.txt  everything the terminal showed (raw)
  answers.txt     each question seen, Kyu's "waiting" note at that moment,
                  and the answer given
Exit status: the command's.

Usage: drive.py OUTDIR PASSWORD -- COMMAND...
"""
import fcntl, os, pty, re, select, struct, sys, termios, time

out, password = sys.argv[1], sys.argv[2]
cmd = sys.argv[sys.argv.index("--") + 1:]
os.makedirs(out, exist_ok=True)
waiting_file = os.path.join(os.environ.get("XDG_STATE_HOME", os.path.expanduser("~/.local/state")), "kyu", "waiting")

pid, fd = pty.fork()
if pid == 0:
    os.execvp(cmd[0], cmd)
# A terminal of 120 × 40, as a window has (pacman lays out its tables by it).
fcntl.ioctl(fd, termios.TIOCSWINSZ, struct.pack("HHHH", 40, 120, 0, 0))

raw = open(os.path.join(out, "transcript.txt"), "wb")
log = open(os.path.join(out, "answers.txt"), "w")
tail = b""
answered_at = 0
ansi = re.compile(rb"\x1b(\[[0-9;?]*[ -/]*[@-~]|\][^\x07\x1b]*(\x07|\x1b\\)|[()][0-9A-Za-z]|[@-Z\\-_])")

def last_line():
    text = ansi.sub(b"", tail).decode("utf-8", "replace")
    return text.split("\n")[-1].split("\r")[-1].strip()

def answer(reply, line):
    # What Kyu noted for the Kyu window at this moment (it notes a question
    # after 1.5 s of silence).
    note = open(waiting_file).read().strip() if os.path.exists(waiting_file) else ""
    log.write(f"question: {line!r}\nwaiting note: {note!r}\nanswer: {reply!r}\n\n")
    log.flush()
    os.write(fd, reply.encode() + b"\r" if reply != "" else b"\r")

last_change = time.time()
while True:
    r, _, _ = select.select([fd], [], [], 0.25)
    if r:
        try:
            data = os.read(fd, 65536)
        except OSError:
            break
        if not data:
            break
        raw.write(data); raw.flush()
        tail = (tail + data)[-8192:]
        last_change = time.time()
        continue
    # Quiet for 2.5 s on a question: answer it (Kyu should have noted it).
    if time.time() - last_change < 2.5 or time.time() - answered_at < 1:
        continue
    line = last_line()
    if not line:
        continue
    reply = None
    if re.search(r"password|\[sudo\]", line, re.I):
        reply = password
    elif re.search(r"\[Y/n\]|\[J/n\]", line):
        reply = "y"
    elif "Continue? [y/N]" in line:
        reply = "y"
    elif re.search(r"Enter a (number|selection)", line):
        reply = ""
    elif "[Enter] close" in ansi.sub(b"", tail[-600:]).decode("utf-8", "replace"):
        # Kyu's last line (anything printed after it on the same screen
        # would be a bug: noted).
        if "[Enter] close" not in line:
            log.write(f"after the last line: {line!r}\n")
        reply = ""
    elif re.search(r"\[y/N\]", line):
        reply = "y"
    if reply is not None:
        answer(reply, line)
        answered_at = time.time()
        tail = b""
    elif time.time() - last_change > 1800:
        log.write(f"stuck on: {line!r}\n"); log.flush()
        os.kill(pid, 9)
        break

_, status = os.waitpid(pid, 0)
code = os.waitstatus_to_exitcode(status)
log.write(f"exit: {code}\n")
sys.exit(code if code >= 0 else 1)
