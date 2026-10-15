import subprocess
import time
import socket
import json
import os
import sys

def main():
    qmp_sock_path = "/home/dwip/Downloads/project/LemonOS/qmp_doom.sock"
    ppm_path = "/home/dwip/Downloads/project/LemonOS/doom_screendump.ppm"
    png_path = "/home/dwip/.gemini/antigravity-ide/brain/21333e56-543f-4d4d-95a5-f16f98eb17ee/doom_e1m1.png"
    assets_png = "/home/dwip/Downloads/project/LemonOS/assets/doom_e1m1.png"

    if os.path.exists(qmp_sock_path):
        os.remove(qmp_sock_path)
    if os.path.exists(ppm_path):
        os.remove(ppm_path)

    cmd = [
        "qemu-system-i386",
        "-kernel", "lemon.bin",
        "-m", "128M",
        "-vga", "std",
        "-device", "sb16",
        "-serial", "file:serial_doom.log",
        "-display", "none",
        "-qmp", f"unix:{qmp_sock_path},server,nowait"
    ]

    print("[Python] Starting QEMU...")
    proc = subprocess.Popen(cmd)

    # Wait for QMP socket to appear
    for _ in range(50):
        if os.path.exists(qmp_sock_path):
            break
        time.sleep(0.1)

    print("[Python] Waiting for Doom to boot and render E1M1...")
    time.sleep(6)

    try:
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

        greeting = read_qmp()
        print("[Python] QMP Greeting:", greeting)

        # qmp_capabilities
        s.sendall(json.dumps({"execute": "qmp_capabilities"}).encode('utf-8') + b"\r\n")
        resp = read_qmp()
        print("[Python] QMP Capabilities Resp:", resp)

        time.sleep(1)

        # screendump initial E1M1
        s.sendall(json.dumps({"execute": "screendump", "arguments": {"filename": ppm_path}}).encode('utf-8') + b"\r\n")
        resp = read_qmp()
        print("[Python] Screendump Resp:", resp)

        # Send Ctrl key (Fire pistol)
        print("[Python] Sending Ctrl key (FIRE!)...")
        s.sendall(json.dumps({"execute": "send-key", "arguments": {"keys": [{"type": "qcode", "data": "ctrl"}]}}).encode('utf-8') + b"\r\n")
        resp = read_qmp()
        print("[Python] Send-key Ctrl Resp:", resp)

        time.sleep(0.3)

        # screendump during weapon fire
        fire_ppm = "/home/dwip/Downloads/project/LemonOS/doom_fire.ppm"
        s.sendall(json.dumps({"execute": "screendump", "arguments": {"filename": fire_ppm}}).encode('utf-8') + b"\r\n")
        resp = read_qmp()
        print("[Python] Screendump Fire Resp:", resp)

        s.close()
    except Exception as e:
        print("[Python] QMP Error:", e)

    proc.terminate()
    try:
        proc.wait(timeout=3)
    except:
        proc.kill()

    if os.path.exists(ppm_path):
        print(f"[Python] PPM dumped successfully ({os.path.getsize(ppm_path)} bytes). Converting to PNG...")
        os.makedirs(os.path.dirname(png_path), exist_ok=True)
        subprocess.run(["convert", ppm_path, png_path], check=True)
        subprocess.run(["convert", ppm_path, assets_png], check=True)
        print(f"[Python] Saved screenshot to {png_path} and {assets_png}!")
    else:
        print("[Python] PPM dump failed!")

    fire_png = "/home/dwip/.gemini/antigravity-ide/brain/21333e56-543f-4d4d-95a5-f16f98eb17ee/doom_fire.png"
    assets_fire_png = "/home/dwip/Downloads/project/LemonOS/assets/doom_fire.png"
    if os.path.exists(fire_ppm):
        subprocess.run(["convert", fire_ppm, fire_png], check=True)
        subprocess.run(["convert", fire_ppm, assets_fire_png], check=True)
        print(f"[Python] Saved fire screenshot to {fire_png}!")

if __name__ == "__main__":
    main()
