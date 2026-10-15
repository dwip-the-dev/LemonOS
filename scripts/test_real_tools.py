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
        elif ch == '"':
            hmp(s, "sendkey shift-apostrophe")
        elif ch == '=':
            hmp(s, "sendkey equal")
        elif ch.isupper():
            hmp(s, f"sendkey shift-{ch.lower()}")
        else:
            hmp(s, f"sendkey {ch}")
        time.sleep(0.04)

def main():
    qmp_sock = "/tmp/qmp_real.sock"
    if os.path.exists(qmp_sock):
        os.remove(qmp_sock)

    cmd = [
        "qemu-system-i386",
        "-kernel", "lemon.bin",
        "-m", "1024M",
        "-vga", "std",
        "-device", "sb16",
        "-netdev", "user,id=u1,dns=8.8.8.8",
        "-device", "e1000,netdev=u1",
        "-object", "filter-dump,id=f1,netdev=u1,file=/tmp/lemon_net.pcap",
        "-serial", "file:serial_real.log",
        "-display", "none",
        "-qmp", f"unix:{qmp_sock},server,nowait"
    ]

    print("[Test] Launching LemonOS in QEMU for Real Tools Verification...")
    proc = subprocess.Popen(cmd)

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
    print("[Test] QMP Ready. Waiting for desktop boot in serial log...")
    for _ in range(100):
        if os.path.exists("serial_real.log"):
            with open("serial_real.log", "r", errors="ignore") as f:
                content = f.read()
                if "Desktop GUI initialized" in content or "IntelliMouse" in content:
                    break
        time.sleep(0.1)
    time.sleep(1.0)

    mouse = MouseTracker(s)

    # Launch Terminal via Desktop Icon (40, 180)
    print("[Test 1] Launching Terminal...")
    mouse.move_to(40, 180)
    time.sleep(0.1)
    mouse.click()
    time.sleep(1.2)

    # Test 1: Real Neofetch
    print("[Test 2] Running real 'neofetch'...")
    send_keys(s, "neofetch\n")
    time.sleep(1.0)
    screendump(s, "real_01_neofetch")

    # Test 2: Real Htop
    print("[Test 3] Running real 'htop'...")
    send_keys(s, "clear\n")
    time.sleep(0.2)
    send_keys(s, "htop\n")
    time.sleep(1.0)
    screendump(s, "real_02_htop")

    # Test 3: Real Tree and LS
    print("[Test 4] Running real 'tree' and 'ls'...")
    send_keys(s, "clear\n")
    time.sleep(0.2)
    send_keys(s, "tree\n")
    time.sleep(0.8)
    screendump(s, "real_03_tree")

    send_keys(s, "clear\n")
    time.sleep(0.2)
    send_keys(s, "ls\n")
    time.sleep(0.8)
    screendump(s, "real_04_ls")

    # Test 4: Real Curl CLI with Baremetal TLS 1.2 HTTPS Request
    print("[Test 5] Running real 'curl -i https://raw.githubusercontent.com/dwip-the-dev/LemonOS/main/README.md'...")
    send_keys(s, "clear\n")
    time.sleep(0.2)
    send_keys(s, "curl -i https://raw.githubusercontent.com/dwip-the-dev/LemonOS/main/README.md\n")
    time.sleep(8.0)
    screendump(s, "real_05_curl")

    # Test 5: Real Wget with Baremetal TLS 1.2 HTTPS Request
    print("[Test 6] Running real 'wget https://raw.githubusercontent.com/dwip-the-dev/LemonOS/main/README.md'...")
    send_keys(s, "clear\n")
    time.sleep(0.2)
    send_keys(s, "wget https://raw.githubusercontent.com/dwip-the-dev/LemonOS/main/README.md\n")
    time.sleep(8.0)
    screendump(s, "real_07_wget")

    # Test 6: Real Ping
    print("[Test 7] Running real 'ping 10.0.2.2'...")
    send_keys(s, "clear\n")
    time.sleep(0.2)
    send_keys(s, "ping 10.0.2.2\n")
    time.sleep(1.2)
    screendump(s, "real_06_ping")

    print("[Done] All real CLI test steps executed.")
    proc.terminate()
    try:
        proc.wait(timeout=2)
    except:
        proc.kill()

if __name__ == "__main__":
    main()
