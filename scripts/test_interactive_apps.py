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

    def read_until(pattern, timeout=5):
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

    def send_cmd(cmd_str):
        print(f"[CMD] {cmd_str}")
        os.write(master, (cmd_str + "\n").encode('utf-8'))
        res = read_until("dwip@lemonos:~$", timeout=6)
        print(res)
        return res

    def send_sql(sql_str):
        print(f"[SQL] {sql_str}")
        os.write(master, (sql_str + "\n").encode('utf-8'))
        res = read_until("sqlite> ", timeout=5)
        print(res)
        return res

    def send_ssh(cmd_str):
        print(f"[SSH] {cmd_str}")
        os.write(master, (cmd_str + "\n").encode('utf-8'))
        res = read_until("dwip@server.local:~$", timeout=5)
        print(res)
        return res

    # Wait for boot and shell prompt
    print("[Waiting for LemonOS to finish booting...]")
    boot_out = read_until("dwip@lemonos:~$", timeout=15)
    print("[Boot complete, shell prompt ready!]")

    # Test busybox
    print("\n--- Testing BusyBox ---")
    send_cmd("busybox uname -a")
    send_cmd("busybox df")

    # Test Lua
    print("\n--- Testing Lua ---")
    send_cmd("lua -e 'print(10 + 20 * 3)'")

    # Test Python
    print("\n--- Testing Python ---")
    send_cmd("python3 -c 'print(100 + 250)'")

    # Test Git
    print("\n--- Testing Git ---")
    send_cmd("git init")
    send_cmd("git status")

    # Test Tar & Gzip
    print("\n--- Testing Tar & Gzip ---")
    send_cmd("tar -cf welcome.tar /docs/welcome.txt")
    send_cmd("tar -tf welcome.tar")
    send_cmd("gzip /docs/welcome.txt")
    send_cmd("gunzip /docs/welcome.txt.gz")

    # Test SQLite
    print("\n--- Testing SQLite ---")
    print("[CMD] sqlite3 test.db")
    os.write(master, b"sqlite3 test.db\n")
    read_until("sqlite> ", timeout=5)
    send_sql("CREATE TABLE users (id, name);")
    send_sql("INSERT INTO users VALUES (1, 'dwip');")
    send_sql("SELECT * FROM users;")
    send_sql(".tables")
    print("[SQL] .quit")
    os.write(master, b".quit\n")
    read_until("dwip@lemonos:~$", timeout=5)

    # Test cURL
    print("\n--- Testing cURL ---")
    send_cmd("curl -v http://dwip.me/api")

    # Test SSH
    print("\n--- Testing SSH ---")
    print("[CMD] ssh server.local")
    os.write(master, b"ssh server.local\n")
    read_until("dwip@server.local:~$", timeout=6)
    print("[SSH] exit")
    os.write(master, b"exit\n")
    read_until("dwip@lemonos:~$", timeout=5)

    proc.terminate()
    try:
        proc.wait(timeout=2)
    except:
        proc.kill()

    os.close(master)
    os.close(slave)

if __name__ == "__main__":
    main()
