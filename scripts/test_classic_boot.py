import subprocess
import time
import socket
import os
import sys

def main():
    sock_in = "/home/dwip/Downloads/project/LemonOS/serial_in.sock"
    log_file = "/home/dwip/Downloads/project/LemonOS/classic_apps_test.log"

    cmd = [
        "qemu-system-i386",
        "-kernel", "lemon.bin",
        "-m", "128M",
        "-vga", "none",
        "-serial", f"file:{log_file}",
        "-display", "none"
    ]

    print("[Test] Starting LemonOS in text mode...")
    proc = subprocess.Popen(cmd)
    time.sleep(3)
    proc.terminate()
    try:
        proc.wait(timeout=2)
    except:
        proc.kill()

    if os.path.exists(log_file):
        with open(log_file, "r", errors="ignore") as f:
            content = f.read()
        print(f"[Test] Log captured ({len(content)} bytes). Tail:")
        lines = content.strip().split("\n")
        for line in lines[-30:]:
            print(line)

if __name__ == "__main__":
    main()
