import os
import pty
import subprocess
import time
import select

master, slave = pty.openpty()
pts = os.ttyname(slave)
cmd = ["qemu-system-i386", "-kernel", "lemon.bin", "-m", "128M", "-vga", "none", "-serial", pts, "-display", "none"]
proc = subprocess.Popen(cmd)

buf = b""
start = time.time()
while time.time() - start < 20:
    r, _, _ = select.select([master], [], [], 0.1)
    if r:
        buf += os.read(master, 2048)
        if b"dwip@lemonos:~$" in buf:
            break

print("[SHELL READY, EXECUTING POSIXTEST]")
os.write(master, b"posixtest\n")
pbuf = b""
start_test = time.time()
while time.time() - start_test < 12:
    r, _, _ = select.select([master], [], [], 0.1)
    if r:
        chunk = os.read(master, 2048)
        pbuf += chunk
        if b"ALL POSIX CONFORMANCE TESTS COMPLETED" in pbuf:
            time.sleep(0.5)
            while True:
                r2, _, _ = select.select([master], [], [], 0.1)
                if r2: pbuf += os.read(master, 2048)
                else: break
            break

print(pbuf.decode("utf-8", errors="ignore"))
proc.terminate()
try:
    proc.wait(timeout=2)
except:
    proc.kill()
