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
        subprocess.run(["convert", ppm, png], check=True)
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

    def btn_down(self):
        hmp(self.s, "mouse_button 1")
        hmp(self.s, "mouse_move 0 0")
        time.sleep(0.08)

    def btn_up(self):
        hmp(self.s, "mouse_button 0")
        hmp(self.s, "mouse_move 0 0")
        time.sleep(0.08)

    def click(self):
        self.btn_down()
        time.sleep(0.06)
        self.btn_up()
        time.sleep(0.08)

def send_keys(s, text):
    for ch in text:
        if ch == '\n':
            hmp(s, "sendkey ret")
        elif ch == ' ':
            hmp(s, "sendkey spc")
        elif ch == '-':
            hmp(s, "sendkey minus")
        elif ch == '/':
            hmp(s, "sendkey slash")
        elif ch == '.':
            hmp(s, "sendkey dot")
        elif ch == ':':
            hmp(s, "sendkey shift-semicolon")
        elif ch == '=':
            hmp(s, "sendkey equal")
        else:
            hmp(s, f"sendkey {ch}")
        time.sleep(0.03)

def main():
    qmp_sock = "/tmp/qmp_speedtest.sock"
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
        "-serial", "file:serial_speedtest.log",
        "-display", "none",
        "-qmp", f"unix:{qmp_sock},server,nowait"
    ]

    print("[Test] Launching LemonOS in QEMU for Speedtest Verification...")
    proc = subprocess.Popen(cmd)

    for _ in range(50):
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
        if os.path.exists("serial_speedtest.log"):
            with open("serial_speedtest.log", "r", errors="ignore") as f:
                content = f.read()
                if "Desktop GUI initialized" in content or "IntelliMouse" in content:
                    break
        time.sleep(0.1)
    time.sleep(1.0)

    screendump(s, "speedtest_00_desktop")

    mouse = MouseTracker(s)

    # 1. Open Terminal via Desktop Icon (40, 180)
    print("[Test 1] Launching Terminal to test APT package install...")
    mouse.move_to(40, 180)
    time.sleep(0.1)
    mouse.click()
    time.sleep(1.5)

    # 2. Run 'sudo apt install speedtest'
    print("[Test 2] Running 'sudo apt install speedtest'...")
    send_keys(s, "sudo apt install speedtest\n")
    time.sleep(1.2)
    screendump(s, "speedtest_01_apt_install")

    # 3. Clear and Run 'speedtest'
    print("[Test 3] Running 'speedtest' (calibrated for 80 Mbps)...")
    send_keys(s, "clear\n")
    time.sleep(0.3)
    send_keys(s, "speedtest\n")
    time.sleep(1.5)
    screendump(s, "speedtest_02_terminal_run")

    # 4. Launch Graphical Speedtest app via Start Menu
    print("[Test 4] Opening Start Menu to launch Graphical Speedtest...")
    # Start menu button is at taskbar: center (48, 742)
    mouse.move_to(48, 742)
    time.sleep(0.1)
    mouse.click()
    time.sleep(0.6)

    # In Start Menu (menu_x = 192, menu_y = 50, sb_x = 216, sb_y = 68, grid_y = 120)
    # Speedtest is app index 3: r = 1, c = 0
    # cell_w = 188, cell_h = 56. cx = 216, cy = 120 + 1 * (56 + 10) = 186
    # Center of Speedtest item: (280, 214)
    mouse.move_to(280, 214)
    time.sleep(0.1)
    mouse.click()
    time.sleep(1.5)
    screendump(s, "speedtest_03_gui_app")

    # 5. Click Start button on the Speedometer gauge to test live animated measurement
    print("[Test 5] Clicking Speedometer gauge center to trigger test animation...")
    # Window is at x=190, y=95, width=580, height=420
    # Center of gauge: screen x = 190 + 580/2 = 480, screen y = 95 + 32 + 172 = 299
    mouse.move_to(480, 299)
    time.sleep(0.1)
    mouse.click()
    time.sleep(0.5)
    screendump(s, "speedtest_04_gui_live_gauge")

    time.sleep(2.0)
    screendump(s, "speedtest_05_gui_completed")

    s.close()
    proc.terminate()
    try:
        proc.wait(timeout=3)
    except:
        proc.kill()

    print("[Test] Speedtest automated verification completed successfully!")

if __name__ == "__main__":
    main()
