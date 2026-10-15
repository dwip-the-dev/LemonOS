import subprocess
import time
import socket
import json
import os

def main():
    qmp_sock_path = "/home/dwip/Downloads/project/LemonOS/qmp_desktop.sock"
    ppm_path = "/home/dwip/Downloads/project/LemonOS/desktop_screendump.ppm"
    png_path = "/home/dwip/.gemini/antigravity-ide/brain/21333e56-543f-4d4d-95a5-f16f98eb17ee/lemonos_desktop_doom.png"
    assets_png = "/home/dwip/Downloads/project/LemonOS/assets/lemonos_desktop_doom.png"

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
        "-serial", "file:serial_desktop.log",
        "-display", "none",
        "-qmp", f"unix:{qmp_sock_path},server,nowait"
    ]

    print("[Python] Starting QEMU for Desktop...")
    proc = subprocess.Popen(cmd)

    for _ in range(50):
        if os.path.exists(qmp_sock_path):
            break
        time.sleep(0.1)

    print("[Python] Waiting for Desktop GUI to initialize...")
    time.sleep(4)

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

        read_qmp()
        s.sendall(json.dumps({"execute": "qmp_capabilities"}).encode('utf-8') + b"\r\n")
        read_qmp()
        time.sleep(1)

        s.sendall(json.dumps({"execute": "screendump", "arguments": {"filename": ppm_path}}).encode('utf-8') + b"\r\n")
        read_qmp()
        s.close()
    except Exception as e:
        print("[Python] QMP Error:", e)

    proc.terminate()
    try:
        proc.wait(timeout=3)
    except:
        proc.kill()

    if os.path.exists(ppm_path):
        subprocess.run(["convert", ppm_path, png_path], check=True)
        subprocess.run(["convert", ppm_path, assets_png], check=True)
        print(f"[Python] Saved desktop screenshot to {png_path} and {assets_png}!")

if __name__ == "__main__":
    main()
