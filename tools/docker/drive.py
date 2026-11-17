#!/usr/bin/env python3
"""Drive kyu-update in a real terminal, as a person would.

Runs the command in a pseudo-terminal (120 x 40), answers what it asks, and
records:
  transcript.txt  everything the terminal showed (raw)
  answers.txt     each question seen, Kyu's "waiting" note at that moment,
                  and the answer given
Exit status: the command's.

How to answer, from the environment (defaults in brackets):
  DRIVE_PASSWORD       sudo's password [kyu]
  DRIVE_WRONG          give a wrong password this many times first [0]
  DRIVE_PROCEED        pacman's "Proceed?": y, n, or ctrlc [y]
  DRIVE_NEWS           Kyu's "Continue? [y/N]": y, n, or ctrlc [y]
  DRIVE_PASSWORD_CTRLC 1: Ctrl+C at the password prompt
  DRIVE_KEYS           keys for Kyu's last lines, one per last line,
                       comma-separated ("f,", "m,"); "" is Enter [Enter]
  DRIVE_PACDIFF        pacdiff's answer [s]
  DRIVE_KILL           kill pacman (SIGKILL) once the output shows this text
  DRIVE_OTHER          any other question: this answer [Enter]

Usage: drive.py OUTDIR -- COMMAND...
"""
import fcntl, os, pty, re, select, signal, struct, subprocess, sys, termios, time

out = sys.argv[1]
cmd = sys.argv[sys.argv.index("--") + 1:]
os.makedirs(out, exist_ok=True)
env = os.environ.get
password = env("DRIVE_PASSWORD", "kyu")
wrong = int(env("DRIVE_WRONG", "0"))
keys = env("DRIVE_KEYS", "").split(",") if env("DRIVE_KEYS") else []
kill_on = env("DRIVE_KILL", "")
waiting_file = os.path.join(env("XDG_STATE_HOME", os.path.expanduser("~/.local/state")), "kyu", "waiting")

pid, fd = pty.fork()
if pid == 0:
    os.execvp(cmd[0], cmd)
fcntl.ioctl(fd, termios.TIOCSWINSZ, struct.pack("HHHH", 40, 120, 0, 0))

raw = open(os.path.join(out, "transcript.txt"), "wb")
log = open(os.path.join(out, "answers.txt"), "w")
tail = b""
seen = b""
answered_at = 0.0
killed = False
ansi = re.compile(rb"\x1b(\[[0-9;?]*[ -/]*[@-~]|\][^\x07\x1b]*(\x07|\x1b\\)|[()][0-9A-Za-z]|[@-Z\\-_])")

def clean(data):
    return ansi.sub(b"", data).decode("utf-8", "replace")

def last_line():
    return clean(tail).split("\n")[-1].split("\r")[-1].strip()

def send(reply, line, what=None):
    note = open(waiting_file).read().strip() if os.path.exists(waiting_file) else ""
    log.write(f"question: {line!r}\nwaiting note: {note!r}\nanswer: {what or reply!r}\n\n")
    log.flush()
    if reply == "\x03":
        os.write(fd, b"\x03")
    else:
        os.write(fd, reply.encode() + b"\r")

def choice(name, default):
    v = env(name, default)
    return "\x03" if v == "ctrlc" else v

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
        seen = (seen + data)[-65536:]
        last_change = time.time()
        if kill_on and not killed and kill_on in clean(seen):
            subprocess.run(["sudo", "-n", "pkill", "-KILL", "-x", "pacman"])
            log.write(f"killed pacman at {kill_on!r}\n\n"); log.flush()
            killed = True
        continue
    # Quiet for 2.5 s on a question: answer it (Kyu should have noted it).
    if time.time() - last_change < 2.5 or time.time() - answered_at < 1:
        continue
    line = last_line()
    reply = None
    what = None
    recent = clean(tail[-600:])
    if line and re.search(r"password|\[sudo\]", line, re.I):
        if env("DRIVE_PASSWORD_CTRLC") == "1":
            reply = "\x03"
        elif wrong > 0:
            wrong -= 1
            reply, what = "wrong", "a wrong password"
        else:
            reply, what = password, "the password"
    elif line and re.search(r"Proceed with installation\? \[[YJ]/n\]|Proceed with download|Fortfahren", line):
        reply = choice("DRIVE_PROCEED", "y")
    elif line and "Continue? [y/N]" in line:
        reply = choice("DRIVE_NEWS", "y")
    elif line and re.search(r"\(V\)iew.*\[v/s/r/o/q\]|\[v/s/r/o/q\]", line):
        reply = env("DRIVE_PACDIFF", "s")
    elif "[Enter] close" in recent:
        if "[Enter] close" not in line:
            log.write(f"after the last line: {line!r}\n")
        reply = keys.pop(0) if keys else ""
        line = line or "(last line)"
    elif line and re.search(r"\[[YJ]/n\]|\[y/N\]|\?\s*$|:\s*$", line):
        reply = env("DRIVE_OTHER", "")
    if reply is not None:
        send(reply, line, what)
        answered_at = time.time()
        tail = b""
    elif time.time() - last_change > 900:
        log.write(f"stuck on: {line!r}\n"); log.flush()
        os.kill(pid, signal.SIGKILL)
        break

_, status = os.waitpid(pid, 0)
raw.close()
# The text as the screen showed it: each line's last drawing.
text = clean(open(os.path.join(out, "transcript.txt"), "rb").read().replace(b"\x1b(B", b""))
lines = []
for line in text.split("\n"):
    parts = [p for p in line.split("\r") if p.strip()]
    lines.append(parts[-1] if parts else "")
open(os.path.join(out, "screen.txt"), "w").write("\n".join(lines))
code = os.waitstatus_to_exitcode(status)
log.write(f"exit: {code}\n")
open(os.path.join(out, "exit"), "w").write(f"{code}\n")
sys.exit(code if code >= 0 else 1)
