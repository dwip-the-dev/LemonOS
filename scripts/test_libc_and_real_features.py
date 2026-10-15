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
        if b"dwip@" in buf and (b"$" in buf or b"#" in buf):
            break

# Drain any residual boot output
time.sleep(0.5)
while True:
    r, _, _ = select.select([master], [], [], 0.1)
    if r:
        os.read(master, 2048)
    else:
        break

def run_cmd(c_str, wait_sec=6):
    # send command slowly char by char or full line
    os.write(master, c_str.encode('utf-8') + b"\n")
    out = b""
    s_time = time.time()
    while time.time() - s_time < wait_sec:
        r, _, _ = select.select([master], [], [], 0.1)
        if r:
            chunk = os.read(master, 2048)
            out += chunk
            if (b"lemonos:~$" in out or b"lemon-box:~$" in out or b"lemonos:~#" in out or b"lemon-box:~#" in out):
                break
    time.sleep(0.2)
    return out.decode('utf-8', errors='ignore')

print("=== 1. TESTING LIBCTEST ===")
out1 = run_cmd("libctest", wait_sec=8)
print(out1)

print("=== 2. TESTING REAL HOSTNAME UPDATE ===")
out2 = run_cmd("sudo hostname lemon-box", wait_sec=4)
print(out2)
out3 = run_cmd("hostname", wait_sec=4)
print(out3)
out_cat = run_cmd("cat /etc/hostname", wait_sec=4)
print(out_cat)

print("=== 3. TESTING DYNLINK / DYNPOSIX ===")
out4 = run_cmd("dynposix", wait_sec=6)
print(out4)

proc.terminate()
try:
    proc.wait(timeout=2)
except:
    proc.kill()

