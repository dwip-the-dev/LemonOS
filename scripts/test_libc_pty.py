import os
import pty
import subprocess
import time
import select

def main():
    master, slave = pty.openpty()

    cmd = [
        "qemu-system-i386",
        "-kernel", "lemon.bin",
        "-m", "128M",
        "-vga", "none",
        "-serial", f"/dev/pts/{os.ttyname(slave).split('/')[-1]}",
        "-display", "none"
    ]

    print("[Python] Spawning QEMU with serial pty...")
    proc = subprocess.Popen(cmd)

    def read_until(pattern, timeout=8):
        buf = b""
        end_time = time.time() + timeout
        while time.time() < end_time:
            r, _, _ = select.select([master], [], [], 0.1)
            if r:
                chunk = os.read(master, 1024)
                if not chunk:
                    break
                buf += chunk
                if pattern.encode('utf-8') in buf:
                    return buf.decode('utf-8', errors='ignore')
        return buf.decode('utf-8', errors='ignore')

    def send_cmd(cmd_str, timeout=8):
        print(f"\n>>> Running: {cmd_str}")
        os.write(master, (cmd_str + "\n").encode('utf-8'))
        res = read_until("dwip@lemonos:~$", timeout=timeout)
        print(res)
        return res

    # Wait for boot and shell prompt
    print("[Waiting for LemonOS to finish booting...]")
    boot_out = read_until("dwip@lemonos:~$", timeout=20)
    print(boot_out)
    print("[Boot complete, shell prompt ready!]")

    # 1. Test posixtest
    send_cmd("posixtest")

    # 2. Test libctest
    send_cmd("libctest")

    # 3. Test dynposix
    send_cmd("dynposix")

    # 4. Test hello, echo, sysinfo, cat
    send_cmd("hello")
    send_cmd("echo Hello from LemonOS Ring 3 POSIX libc!")
    send_cmd("sysinfo")
    send_cmd("cat /docs/welcome.txt")

    # 5. Check /usr/include
    send_cmd("ls /usr/include")

    proc.terminate()
    try:
        proc.wait(timeout=2)
    except:
        proc.kill()

if __name__ == "__main__":
    main()
