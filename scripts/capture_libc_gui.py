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
        elif ch.isupper():
            hmp(s, f"sendkey shift-{ch.lower()}")
        else:
            hmp(s, f"sendkey {ch}")
        time.sleep(0.04)
    time.sleep(0.2)

def main():
    qmp_sock_path = "/home/dwip/Downloads/project/LemonOS/qmp_libc.sock"
    if os.path.exists(qmp_sock_path):
        os.remove(qmp_sock_path)

    cmd = [
        "qemu-system-i386",
        "-kernel", "lemon.bin",
        "-m", "256M",
        "-vga", "std",
        "-device", "sb16",
        "-serial", "file:serial_libc_gui.log",
        "-display", "none",
        "-qmp", f"unix:{qmp_sock_path},server,nowait"
    ]

    print("[Python] Starting QEMU with Desktop GUI...")
    proc = subprocess.Popen(cmd)

    for _ in range(60):
        if os.path.exists(qmp_sock_path):
            break
        time.sleep(0.1)

    print("[Python] Waiting for Desktop GUI...")
    time.sleep(3.5)

    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.connect(qmp_sock_path)

    def read_qmp():
        data = b""
        while b"\r\n" not in data:
            chunk = s.recv(1024)
            if not chunk:
                break
            data += chunk
        return json.loads(data.decode('utf-8'))

    read_qmp()
    s.sendall(json.dumps({"execute": "qmp_capabilities"}).encode('utf-8') + b"\r\n")
    read_qmp()
    time.sleep(0.5)

    mouse = MouseTracker(s)

    # Launch Terminal from desktop icon (40, 180)
    print("[Action] Launching Terminal window...")
    mouse.move_to(40, 180)
    time.sleep(0.1)
    mouse.click()
    time.sleep(1.0)

    # 1. Run posixtest
    print("[Action] Running posixtest in GUI terminal...")
    send_keys(s, "posixtest\n")
    time.sleep(2.5)
    screendump(s, "libc_posix_gui")

    # 2. Run libctest
    print("[Action] Running libctest in GUI terminal...")
    send_keys(s, "libctest\n")
    time.sleep(2.5)
    screendump(s, "libc_c99_gui")

    # 3. Run dynposix
    print("[Action] Running dynposix in GUI terminal...")
    send_keys(s, "dynposix\n")
    time.sleep(2.0)
    screendump(s, "libc_dynposix_gui")

    # 4. Run standard C CLI utilities
    print("[Action] Running hello and sysinfo in GUI terminal...")
    send_keys(s, "hello\n")
    time.sleep(1.0)
    send_keys(s, "sysinfo\n")
    time.sleep(1.5)
    screendump(s, "libc_apps_gui")

    print("[Done] Screenshots captured successfully.")
    proc.terminate()
    try:
        proc.wait(timeout=2)
    except:
        proc.kill()

if __name__ == "__main__":
    main()
