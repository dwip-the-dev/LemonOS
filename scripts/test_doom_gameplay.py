import subprocess
import time
import socket
import json
import os
import sys

def main():
    qmp_sock = "/tmp/qmp_doom_play.sock"
    artifacts_dir = "/home/dwip/.gemini/antigravity-ide/brain/21333e56-543f-4d4d-95a5-f16f98eb17ee"
    assets_dir = "/home/dwip/Downloads/project/LemonOS/assets"

    os.makedirs(artifacts_dir, exist_ok=True)
    os.makedirs(assets_dir, exist_ok=True)

    if os.path.exists(qmp_sock):
        os.remove(qmp_sock)

    print("[DoomTest] Launching LemonOS in QEMU with BGA and SB16...")
    cmd = [
        "qemu-system-i386",
        "-kernel", "lemon.bin",
        "-m", "128M",
        "-vga", "std",
        "-device", "sb16",
        "-serial", "stdio",
        "-display", "none",
        "-qmp", f"unix:{qmp_sock},server,nowait"
    ]

    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    for _ in range(50):
        if os.path.exists(qmp_sock):
            break
        time.sleep(0.1)

    time.sleep(3)
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.connect(qmp_sock)

    def qmp_cmd(cmd_dict):
        s.sendall(json.dumps(cmd_dict).encode('utf-8') + b"\r\n")
        data = b""
        while b"\r\n" not in data:
            chunk = s.recv(1024)
            if not chunk:
                break
            data += chunk
        return json.loads(data.decode('utf-8', errors='ignore'))

    greeting = json.loads(s.recv(1024).decode('utf-8', errors='ignore'))
    print("[DoomTest] QMP greeting connected.")
    qmp_cmd({"execute": "qmp_capabilities"})

    def screendump_and_convert(name):
        ppm_file = f"/tmp/{name}.ppm"
        if os.path.exists(ppm_file):
            os.remove(ppm_file)
        qmp_cmd({"execute": "screendump", "arguments": {"filename": ppm_file}})
        time.sleep(0.2)
        art_png = os.path.join(artifacts_dir, f"{name}.png")
        asset_png = os.path.join(assets_dir, f"{name}.png")
        if os.path.exists(ppm_file):
            subprocess.run(["magick", ppm_file, art_png], check=True)
            subprocess.run(["magick", ppm_file, asset_png], check=True)
            print(f"[DoomTest] Captured: {name}.png ({os.path.getsize(art_png)} bytes)")
            return ppm_file
        else:
            print(f"[DoomTest] Failed to capture: {name}.ppm")
            return None

    def read_ppm_pixels(path):
        if not path or not os.path.exists(path):
            return None
        with open(path, "rb") as f:
            data = f.read()
        header_end = data.find(b"255\n") + 4
        return data[header_end:]

    def count_diffs(ppm1, ppm2):
        b1 = read_ppm_pixels(ppm1)
        b2 = read_ppm_pixels(ppm2)
        if not b1 or not b2:
            return 0
        diff = 0
        for y in range(184, 584):
            for x in range(192, 832):
                idx = (y * 1024 + x) * 3
                if (b1[idx], b1[idx+1], b1[idx+2]) != (b2[idx], b2[idx+1], b2[idx+2]):
                    diff += 1
        return diff

    # 1. Capture clean desktop
    print("\n--- Phase 1: Verifying Clean Desktop ---")
    ppm_desk = screendump_and_convert("play_0_desktop")

    # 2. Click DOOM desktop icon at (130, 600)
    print("\n--- Phase 2: Launching DOOM from Desktop ---")
    print("[DoomTest] Moving mouse cursor to DOOM icon and clicking...")
    qmp_cmd({
        "execute": "input-send-event",
        "arguments": {
            "events": [
                {"type": "rel", "data": {"axis": "x", "value": -382}},
                {"type": "rel", "data": {"axis": "y", "value": 216}}
            ]
        }
    })
    time.sleep(0.2)
    qmp_cmd({
        "execute": "input-send-event",
        "arguments": {
            "events": [
                {"type": "btn", "data": {"button": "left", "down": True}}
            ]
        }
    })
    time.sleep(0.1)
    qmp_cmd({
        "execute": "input-send-event",
        "arguments": {
            "events": [
                {"type": "btn", "data": {"button": "left", "down": False}}
            ]
        }
    })

    print("[DoomTest] Waiting for DOOM engine to boot into E1M1...")
    time.sleep(6)

    ppm_start = screendump_and_convert("play_1_start")

    # 3. Test Movement: Walk Forward ('w' down for 1.2s)
    print("\n--- Phase 3: Walking Forward (Key 'W') ---")
    qmp_cmd({
        "execute": "input-send-event",
        "arguments": {
            "events": [
                {"type": "key", "data": {"key": {"type": "qcode", "data": "w"}, "down": True}}
            ]
        }
    })
    time.sleep(1.2)
    qmp_cmd({
        "execute": "input-send-event",
        "arguments": {
            "events": [
                {"type": "key", "data": {"key": {"type": "qcode", "data": "w"}, "down": False}}
            ]
        }
    })
    time.sleep(0.4)
    ppm_walk = screendump_and_convert("play_2_walk_forward")
    diff_walk = count_diffs(ppm_start, ppm_walk)
    print(f"[DoomTest] Movement Check: {diff_walk} pixels changed in 3D viewport!")
    assert diff_walk > 20000, f"Player did not move forward! Only {diff_walk} pixels changed."

    # 4. Test Turning: Turn Right ('d' down for 0.8s)
    print("\n--- Phase 4: Turning Right (Key 'D') ---")
    qmp_cmd({
        "execute": "input-send-event",
        "arguments": {
            "events": [
                {"type": "key", "data": {"key": {"type": "qcode", "data": "d"}, "down": True}}
            ]
        }
    })
    time.sleep(0.8)
    qmp_cmd({
        "execute": "input-send-event",
        "arguments": {
            "events": [
                {"type": "key", "data": {"key": {"type": "qcode", "data": "d"}, "down": False}}
            ]
        }
    })
    time.sleep(0.4)
    ppm_turn = screendump_and_convert("play_3_turn_right")
    diff_turn = count_diffs(ppm_walk, ppm_turn)
    print(f"[DoomTest] Turn Check: {diff_turn} pixels changed after turning right!")
    assert diff_turn > 20000, f"Player did not turn right! Only {diff_turn} pixels changed."

    # 5. Test Firing Weapon: Send Ctrl held down for 0.4s to capture muzzle flash & fire animation
    print("\n--- Phase 5: Firing Pistol (Key 'Ctrl' / Left Click) ---")
    qmp_cmd({
        "execute": "input-send-event",
        "arguments": {
            "events": [
                {"type": "key", "data": {"key": {"type": "qcode", "data": "ctrl"}, "down": True}}
            ]
        }
    })
    time.sleep(0.4)
    ppm_fire = screendump_and_convert("play_4_fire_pistol")
    time.sleep(0.1)
    qmp_cmd({
        "execute": "input-send-event",
        "arguments": {
            "events": [
                {"type": "key", "data": {"key": {"type": "qcode", "data": "ctrl"}, "down": False}}
            ]
        }
    })
    time.sleep(0.5)
    diff_fire = count_diffs(ppm_turn, ppm_fire)
    print(f"[DoomTest] Weapon Fire Check: {diff_fire} pixels changed during muzzle flash & pistol fire!")
    assert diff_fire > 20000, f"No muzzle flash or fire animation! Only {diff_fire} pixels changed."

    # 6. Test Pause / Options Menu (Escape key)
    print("\n--- Phase 6: Testing Menu (Escape Key) ---")
    qmp_cmd({"execute": "send-key", "arguments": {"keys": [{"type": "qcode", "data": "esc"}]}})
    time.sleep(0.5)
    ppm_menu = screendump_and_convert("play_5_menu")
    diff_menu = count_diffs(ppm_fire, ppm_menu)
    print(f"[DoomTest] Menu Overlay Check: {diff_menu} pixels changed when opening Doom menu!")
    assert diff_menu > 20000, f"Doom menu did not open! Only {diff_menu} pixels changed."

    # 7. Resume Game (Escape key again)
    print("\n--- Phase 7: Resuming Game (Escape Key) ---")
    qmp_cmd({"execute": "send-key", "arguments": {"keys": [{"type": "qcode", "data": "esc"}]}})
    time.sleep(0.5)
    ppm_resume = screendump_and_convert("play_6_resume")
    diff_resume = count_diffs(ppm_menu, ppm_resume)
    print(f"[DoomTest] Resume Check: {diff_resume} pixels changed when closing menu!")
    assert diff_resume > 20000, f"Doom game did not resume! Only {diff_resume} pixels changed."

    s.close()
    proc.terminate()
    try:
        proc.wait(timeout=3)
    except:
        proc.kill()

    print("\n=======================================================")
    print(" ALL DOOM GAMEPLAY CONTROLS & GRAPHICS VERIFIED 100%!")
    print("=======================================================")

if __name__ == "__main__":
    main()
