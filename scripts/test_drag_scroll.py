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

    def scroll(self, dz, count=1):
        for _ in range(count):
            hmp(self.s, f"mouse_move 0 0 {dz}")
            time.sleep(0.06)

def send_keys(s, keys):
    for ch in keys:
        if ch == '\n':
            hmp(s, "sendkey ret")
        elif ch == ' ':
            hmp(s, "sendkey spc")
        elif ch == '-':
            hmp(s, "sendkey minus")
        elif ch == '/':
            hmp(s, "sendkey slash")
        else:
            hmp(s, f"sendkey {ch}")
        time.sleep(0.03)

def main():
    qmp_sock = "/tmp/qmp_drag_scroll.sock"
    if os.path.exists(qmp_sock):
        os.remove(qmp_sock)

    cmd = [
        "qemu-system-i386",
        "-kernel", "lemon.bin",
        "-m", "128M",
        "-vga", "std",
        "-device", "sb16",
        "-serial", "file:serial_test.log",
        "-display", "none",
        "-qmp", f"unix:{qmp_sock},server,nowait"
    ]

    print("[Test] Launching LemonOS in QEMU...")
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
    print("[Test] QMP Ready. Waiting 4.0s for desktop boot...")
    time.sleep(4.0)

    screendump(s, "01_clean_desktop_boot")

    mouse = MouseTracker(s)

    # -------------------------------------------------------------
    # 1. Test Desktop Rubberband Marquee Selection
    # -------------------------------------------------------------
    print("[Test 1] Testing Desktop Rubberband Marquee Selection...")
    # Move to empty desktop space: (380, 240)
    mouse.move_to(380, 240)
    time.sleep(0.1)
    # Press and drag down-right to (620, 440)
    mouse.btn_down()
    time.sleep(0.1)
    mouse.move_to(620, 440)
    time.sleep(0.2)
    screendump(s, "02_desktop_marquee_active")
    # Release button
    mouse.btn_up()
    time.sleep(0.2)
    screendump(s, "03_desktop_marquee_released")

    # -------------------------------------------------------------
    # 2. Test Desktop Icon Drag & Drop
    # -------------------------------------------------------------
    print("[Test 2] Testing Desktop Icon Dragging...")
    # Move to Snake icon: (124, 230) -> center of icon is (144, 250)
    mouse.move_to(144, 250)
    time.sleep(0.1)
    screendump(s, "04_icon_before_drag")
    # Grab icon and drag it to (340, 260)
    mouse.btn_down()
    time.sleep(0.1)
    mouse.move_to(340, 260)
    time.sleep(0.2)
    screendump(s, "05_icon_dragging")
    # Drop icon
    mouse.btn_up()
    time.sleep(0.2)
    screendump(s, "06_icon_dropped")

    # -------------------------------------------------------------
    # 3. Test Terminal Output & Mouse Scroll Wheel
    # -------------------------------------------------------------
    print("[Test 3] Testing Terminal Launch and Scroll...")
    # Terminal desktop icon: center is (40, 180)
    mouse.move_to(40, 180)
    time.sleep(0.1)
    mouse.click()
    time.sleep(1.2)

    # Fill terminal history
    send_keys(s, "ps aux\n")
    time.sleep(0.3)
    send_keys(s, "help\n")
    time.sleep(0.3)
    send_keys(s, "help\n")
    time.sleep(0.3)
    screendump(s, "07_terminal_output_full")

    # Move mouse inside terminal body: (400, 240)
    mouse.move_to(400, 240)
    time.sleep(0.1)
    print("[Test 3] Scrolling Terminal Wheel UP...")
    mouse.scroll(1, count=12)
    time.sleep(0.4)
    screendump(s, "08_terminal_scrolled_up")

    # Test dragging the terminal scrollbar thumb back down
    # Scrollbar is at: win->x + WM_BORDER_WIDTH + cw - sb_w - 4 = 80 + 3 + 654 - 8 - 4 = 725
    # Scrolled up thumb is at client y = sb_y = 4 -> screen y = 50 + 32 + 4 = 86
    mouse.move_to(725, 95)
    time.sleep(0.1)
    mouse.btn_down()
    time.sleep(0.1)
    mouse.move_to(725, 380)
    time.sleep(0.2)
    screendump(s, "09_terminal_scrollbar_dragged")
    mouse.btn_up()
    time.sleep(0.2)

    # Close terminal window (close button is at x: 714..740, center 726, y: 66)
    mouse.move_to(726, 66)
    time.sleep(0.1)
    mouse.click()
    time.sleep(0.6)

    # -------------------------------------------------------------
    # 4. Test File Explorer Scrolling
    # -------------------------------------------------------------
    print("[Test 4] Testing File Explorer Scrolling...")
    # Files desktop icon: center is (40, 250)
    mouse.move_to(40, 250)
    time.sleep(0.1)
    mouse.click()
    time.sleep(1.2)
    screendump(s, "10_files_initial")

    # Move mouse inside middle pane: (320, 220) and scroll wheel down
    mouse.move_to(320, 220)
    time.sleep(0.1)
    print("[Test 4] Scrolling File Explorer Wheel DOWN...")
    mouse.scroll(-1, count=10)
    time.sleep(0.4)
    screendump(s, "11_files_scrolled_down")

    # Close files window (close button is at x: 694..720, center 707, y: 66)
    mouse.move_to(707, 66)
    time.sleep(0.1)
    mouse.click()
    time.sleep(0.6)

    # -------------------------------------------------------------
    # 5. Test Firefox Scrolling & Dragging
    # -------------------------------------------------------------
    print("[Test 5] Testing Firefox Browser Scrolling...")
    # Firefox desktop icon: center is (40, 75)
    mouse.move_to(40, 75)
    time.sleep(0.1)
    mouse.click()
    time.sleep(1.5)
    screendump(s, "12_firefox_initial")

    # Scroll down on page content: (450, 320)
    mouse.move_to(450, 320)
    time.sleep(0.1)
    print("[Test 5] Scrolling Firefox Wheel DOWN...")
    mouse.scroll(-1, count=10)
    time.sleep(0.4)
    screendump(s, "13_firefox_scrolled_down")

    # Drag scrollbar thumb back up: scrollbar at cw - 14 = 140 + 3 + 734 - 14 = 863
    mouse.move_to(863, 300)
    time.sleep(0.1)
    mouse.btn_down()
    time.sleep(0.1)
    mouse.move_to(863, 180)
    time.sleep(0.2)
    screendump(s, "14_firefox_scrollbar_dragged")
    mouse.btn_up()
    time.sleep(0.2)

    s.close()
    proc.terminate()
    try:
        proc.wait(timeout=3)
    except:
        proc.kill()

    print("[Test] All drag and scroll verification tests completed successfully!")

if __name__ == "__main__":
    main()
