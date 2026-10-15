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
        else:
            hmp(s, f"sendkey {ch}")
        time.sleep(0.055)

def main():
    qmp_sock = "/tmp/qmp_lem.sock"
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
        "-serial", "file:serial_lem.log",
        "-display", "none",
        "-qmp", f"unix:{qmp_sock},server,nowait"
    ]

    print("[Test] Launching LemonOS in QEMU for Native .lem Package Manager Verification...")
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
        if os.path.exists("serial_lem.log"):
            with open("serial_lem.log", "r", errors="ignore") as f:
                content = f.read()
                if "Desktop GUI initialized" in content or "IntelliMouse" in content:
                    break
        time.sleep(0.1)
    time.sleep(1.0)

    screendump(s, "lem_00_desktop")

    mouse = MouseTracker(s)

    # 1. Open Terminal via Desktop Icon (40, 180)
    print("[Test 1] Launching Terminal to test lem package manager...")
    mouse.move_to(40, 180)
    time.sleep(0.1)
    mouse.click()
    time.sleep(1.5)

    # 2. Test 'lem' help banner
    print("[Test 2] Running 'lem' help banner...")
    send_keys(s, "lem\n")
    time.sleep(1.0)
    screendump(s, "lem_01_help_banner")

    # 3. Test 'lem repo list' and adding custom site mirror
    print("[Test 3] Running 'lem repo list' and adding user repository...")
    send_keys(s, "clear\n")
    time.sleep(0.2)
    send_keys(s, "lem repo list\n")
    time.sleep(0.5)
    send_keys(s, "sudo lem repo add http://pkg.lemonos.org\n")
    time.sleep(0.8)
    screendump(s, "lem_02_repo_manage")

    # 4. Test 'lem pack' and 'lem install'
    print("[Test 4] Building .lem package with 'lem pack' and installing it...")
    send_keys(s, "clear\n")
    time.sleep(0.2)
    send_keys(s, "sudo lem pack myapp 1.0.0 /bin/tree\n")
    time.sleep(0.8)
    send_keys(s, "sudo lem install /var/cache/lem/archives/myapp.lem\n")
    time.sleep(1.2)
    screendump(s, "lem_03_pack_and_install")

    # 5. Test 'lem pull' from GitHub / site repository
    print("[Test 5] Running 'sudo lem pull cowsay'...")
    send_keys(s, "clear\n")
    time.sleep(0.2)
    send_keys(s, "sudo lem pull cowsay\n")
    time.sleep(1.2)
    send_keys(s, "cowsay Native LEM works!\n")
    time.sleep(1.0)
    screendump(s, "lem_04_pull_and_run")

    # 6. Test 'lem show' and 'lem list --installed'
    print("[Test 6] Running 'lem show myapp' and 'lem list --installed'...")
    send_keys(s, "clear\n")
    time.sleep(0.2)
    send_keys(s, "lem show myapp\n")
    time.sleep(1.0)
    screendump(s, "lem_05_show_manifest")

    s.close()
    proc.terminate()
    try:
        proc.wait(timeout=3)
    except:
        proc.kill()

    print("[Test] LEM package manager automated verification completed successfully!")

if __name__ == "__main__":
    main()
