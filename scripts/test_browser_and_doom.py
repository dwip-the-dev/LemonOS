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
            time.sleep(0.02)
        time.sleep(0.05)

    def btn_down(self):
        hmp(self.s, "mouse_button 1")
        hmp(self.s, "mouse_move 0 0")
        time.sleep(0.06)

    def btn_up(self):
        hmp(self.s, "mouse_button 0")
        hmp(self.s, "mouse_move 0 0")
        time.sleep(0.06)

    def click(self):
        self.btn_down()
        time.sleep(0.05)
        self.btn_up()
        time.sleep(0.08)

def main():
    qmp_sock = f"/tmp/qmp_{os.getpid()}_{int(time.time())}.sock"
    if os.path.exists(qmp_sock):
        try: os.remove(qmp_sock)
        except: pass

    cmd = [
        "qemu-system-i386",
        "-kernel", "lemon.bin",
        "-m", "1024M",
        "-vga", "std",
        "-device", "sb16",
        "-net", "nic,model=e1000",
        "-net", "user",
        "-serial", "file:serial_browser_doom.log",
        "-display", "none",
        "-qmp", f"unix:{qmp_sock},server,nowait"
    ]

    print("[Test] Launching LemonOS in QEMU for Browser & DOOM Verification...")
    proc = subprocess.Popen(cmd)

    print("[Test] Connecting to QMP socket...")
    s = None
    for _ in range(50):
        if proc.poll() is not None:
            print("[Error] QEMU exited early with code:", proc.poll())
            break
        if os.path.exists(qmp_sock):
            try:
                s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
                s.connect(qmp_sock)
                break
            except Exception as e:
                if s: s.close()
                s = None
        time.sleep(0.2)
    if not s:
        print("[Error] Failed to connect to QMP socket")
        return

    def read_banner():
        data = b""
        while b"\r\n" not in data:
            data += s.recv(1024)
        return json.loads(data.decode('utf-8'))

    read_banner()
    send_qmp_cmd(s, {"execute": "qmp_capabilities"})
    print("[Test] QMP connected. Waiting for desktop GUI boot...")
    for _ in range(80):
        if os.path.exists("serial_browser_doom.log"):
            try:
                with open("serial_browser_doom.log", "r") as f:
                    if "IntelliMouse" in f.read():
                        break
            except: pass
        time.sleep(0.1)
    time.sleep(1.5)

    mouse = MouseTracker(s)

    # 1. Capture clean desktop
    print("\n[Step 1] Desktop Boot Verification...")
    screendump(s, "browser_01_desktop")

    # 2. Launch Google Browser via Desktop Icon at (40, 75)
    print("\n[Step 2] Launching NetSurf Web Browser via Desktop Icon (40, 75)...")
    mouse.move_to(40, 75)
    time.sleep(0.1)
    mouse.click()
    time.sleep(2.0)
    screendump(s, "browser_02_google_home")

    # 3. Perform Google Search: Search for "LemonOS 3.0" via hotkey '8'
    print("\n[Step 3] Executing Google Search for LemonOS 3.0...")
    hmp(s, "sendkey 8")
    time.sleep(1.2)
    screendump(s, "browser_03_google_serp")

    # 4. Open Wikipedia via Bookmark (hotkey '2')
    print("\n[Step 4] Navigating to Wikipedia Article...")
    hmp(s, "sendkey 2")
    time.sleep(1.0)
    screendump(s, "browser_04_wikipedia")

    # 4b. Scroll down Wikipedia (down arrow key & page down)
    print("\n[Step 4b] Scrolling Down Wikipedia Article using Arrow Keys...")
    hmp(s, "sendkey down")
    time.sleep(0.1)
    hmp(s, "sendkey down")
    time.sleep(0.1)
    hmp(s, "sendkey pgdn")
    time.sleep(0.5)
    screendump(s, "browser_04b_wikipedia_scrolled")

    # 5. Open GitHub via Bookmark (hotkey '3')
    print("\n[Step 5] Navigating to GitHub Repository...")
    hmp(s, "sendkey 3")
    time.sleep(1.0)
    screendump(s, "browser_05_github")

    # 6. Open Speedtest via Bookmark (hotkey '5')
    print("\n[Step 6] Navigating to Speedtest 80 Mbps Web App...")
    hmp(s, "sendkey 5")
    time.sleep(1.0)
    screendump(s, "browser_06_speedtest")

    # 7. Test Alt+F4 to Close Browser Window
    print("\n[Step 7] Testing Alt+F4 to Close Browser Window...")
    hmp(s, "sendkey alt-f4")
    time.sleep(1.0)
    screendump(s, "browser_07_alt_f4_window_closed")

    # 8. Launch DOOM via Start Menu (F1 -> click DOOM tile at 712, 346)
    print("\n[Step 8] Launching DOOM via Start Menu (F1 -> 712, 346)...")
    hmp(s, "sendkey f1")
    time.sleep(0.5)
    mouse.move_to(712, 346)
    time.sleep(0.1)
    mouse.click()
    print("[Test] Waiting 3.5s for DOOM to load level...")
    time.sleep(3.5)
    screendump(s, "browser_08_doom_running")

    # 9. Test Alt+F4 inside DOOM to Exit Cleanly
    print("\n[Step 9] Pressing Alt+F4 inside DOOM to Exit Cleanly...")
    hmp(s, "sendkey alt-f4")
    time.sleep(1.5)
    screendump(s, "browser_09_doom_exited_desktop_restored")

    # Clean shutdown
    print("\n[Test] All tests completed! Terminating QEMU...")
    try:
        send_qmp_cmd(s, {"execute": "quit"})
    except Exception:
        pass
    proc.terminate()
    proc.wait()
    print("[Test] Test run successfully finished.")

if __name__ == "__main__":
    main()
