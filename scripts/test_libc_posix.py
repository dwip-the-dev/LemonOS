import subprocess
import time
import socket
import json
import os

ARTIFACT_DIR = "/home/dwip/.gemini/antigravity-ide/brain/21333e56-543f-4d4d-95a5-f16f98eb17ee"

def send_qmp_cmd(s, cmd_dict):
    s.sendall(json.dumps(cmd_dict).encode('utf-8') + b"\r\n")
    data = b""
    while b"\r\n" not in data:
        chunk = s.recv(1024)
        if not chunk:
            break
        data += chunk
    return json.loads(data.decode('utf-8'))

def hmp(s, cmd_str):
    return send_qmp_cmd(s, {"execute": "human-monitor-command", "arguments": {"command-line": cmd_str}})

def screendump(s, name):
    ppm = f"/tmp/{name}.ppm"
    png = os.path.join(ARTIFACT_DIR, f"{name}.png")
    if os.path.exists(ppm):
        os.remove(ppm)
    send_qmp_cmd(s, {"execute": "screendump", "arguments": {"filename": ppm}})
    time.sleep(0.3)
    if os.path.exists(ppm):
        subprocess.run(["magick", ppm, png], check=True)
        print(f"[Verified Screenshot] Saved: {png}")
        return png
    else:
        print(f"[Error] Failed to dump screen {name}")
        return None

class MouseTracker:
    def __init__(self, s):
        self.s = s
        self.x = 512
        self.y = 384

    def move_to(self, target_x, target_y):
        dx = target_x - self.x
        dy = target_y - self.y
        while dx != 0 or dy != 0:
            step_x = max(-20, min(20, dx))
            step_y = max(-20, min(20, dy))
            hmp(self.s, f"mouse_move {step_x} {step_y}")
            self.x += step_x
            self.y += step_y
            dx = target_x - self.x
            dy = target_y - self.y
            time.sleep(0.035)
        time.sleep(0.08)

    def click(self):
        hmp(self.s, "mouse_button 1")
        hmp(self.s, "mouse_move 0 0")
        time.sleep(0.08)
        hmp(self.s, "mouse_button 0")
        hmp(self.s, "mouse_move 0 0")
        time.sleep(0.08)

def send_keys(s, text):
    for ch in text:
        if ch == '\n':
            hmp(s, "sendkey ret")
        elif ch == ' ':
            hmp(s, "sendkey spc")
        elif ch == '-':
            hmp(s, "sendkey minus")
        elif ch == '_':
            hmp(s, "sendkey shift-minus")
        elif ch == '/':
            hmp(s, "sendkey slash")
        elif ch == '.':
            hmp(s, "sendkey dot")
        elif ch == ':':
            hmp(s, "sendkey shift-semicolon")
        elif ch == '!':
            hmp(s, "sendkey shift-1")
        elif ch.isupper():
            hmp(s, f"sendkey shift-{ch.lower()}")
        else:
            hmp(s, f"sendkey {ch}")
        time.sleep(0.04)
    time.sleep(0.2)

def main():
    qmp_sock = "/tmp/qmp_libc.sock"
    if os.path.exists(qmp_sock):
        os.remove(qmp_sock)

    cmd = [
        "qemu-system-i386",
        "-kernel", "lemon.bin",
        "-m", "1024M",
        "-vga", "std",
        "-device", "sb16",
        "-net", "nic,model=e1000",
        "-net", "user",
        "-serial", "file:serial_libc.log",
        "-display", "none",
        "-qmp", f"unix:{qmp_sock},server,nowait"
    ]

    print("[Test] Launching LemonOS in QEMU for Libc & POSIX Verification...")
    proc = subprocess.Popen(cmd, stderr=subprocess.DEVNULL)

    for _ in range(80):
        if os.path.exists(qmp_sock):
            break
        time.sleep(0.1)

    print("[Test] Connecting to QMP...")
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.connect(qmp_sock)

    def read_banner():
        data = b""
        while b"\r\n" not in data:
            data += s.recv(1024)
        return json.loads(data.decode('utf-8'))

    read_banner()
    send_qmp_cmd(s, {"execute": "qmp_capabilities"})
    print("[Test] QMP Ready. Waiting for desktop GUI boot in serial log...")
    for _ in range(100):
        if os.path.exists("serial_libc.log"):
            with open("serial_libc.log", "r", errors="ignore") as f:
                content = f.read()
                if "Desktop GUI initialized" in content or "IntelliMouse" in content:
                    break
        time.sleep(0.1)
    time.sleep(1.0)

    mouse = MouseTracker(s)

    # 1. Launch Terminal via Desktop Icon (40, 180)
    print("\n[Step 1] Opening Terminal on LemonOS Desktop...")
    mouse.move_to(40, 180)
    time.sleep(0.1)
    mouse.click()
    time.sleep(1.2)
    screendump(s, "libc_01_terminal_opened")

    # 2. Run posixtest
    print("\n[Step 2] Executing 'posixtest' in Ring 3 (fork, pipe, stat, dirent, getcwd, getpid)...")
    send_keys(s, "posixtest\n")
    time.sleep(2.0)
    screendump(s, "libc_02_posixtest")

    # 3. Run libctest
    print("\n[Step 3] Executing 'libctest' in Ring 3 (malloc, free, realloc, qsort, snprintf, math, setjmp)...")
    send_keys(s, "clear\n")
    time.sleep(0.2)
    send_keys(s, "libctest\n")
    time.sleep(2.0)
    screendump(s, "libc_03_libctest")

    # 4. Run dynposix
    print("\n[Step 4] Executing 'dynposix' (dynamically linked against libc.so)...")
    send_keys(s, "clear\n")
    time.sleep(0.2)
    send_keys(s, "dynposix\n")
    time.sleep(1.5)
    screendump(s, "libc_04_dynposix")

    # 5. Run hello, echo, sysinfo
    print("\n[Step 5] Executing upgraded Standard C utilities (hello, echo, sysinfo)...")
    send_keys(s, "clear\n")
    time.sleep(0.2)
    send_keys(s, "hello\n")
    time.sleep(1.0)
    send_keys(s, "echo Real POSIX Libc active in LemonOS!\n")
    time.sleep(1.0)
    send_keys(s, "sysinfo\n")
    time.sleep(1.0)
    screendump(s, "libc_05_standard_utils")

    # 6. Check /usr/include headers
    print("\n[Step 6] Inspecting installed standard C headers in /usr/include...")
    send_keys(s, "ls /usr/include\n")
    time.sleep(1.0)
    screendump(s, "libc_06_usr_include")

    print("\n[Test Complete] Terminating QEMU...")
    proc.terminate()
    try:
        proc.wait(timeout=2)
    except Exception:
        proc.kill()

    print("\n================== SERIAL LOG OUTPUT (Tail) ==================")
    if os.path.exists("serial_libc.log"):
        with open("serial_libc.log", "r", errors="ignore") as f:
            lines = f.readlines()
            for line in lines[-120:]:
                print(line, end="")
    print("\n==============================================================")

if __name__ == "__main__":
    main()
