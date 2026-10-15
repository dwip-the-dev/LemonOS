#!/usr/bin/env python3
import os

apps = [
    ("hello", "user/hello.elf"),
    ("echo", "user/echo.elf"),
    ("sysinfo", "user/sysinfo.elf"),
    ("cat", "user/cat.elf"),
    ("usertest", "user/usertest.elf"),
    ("posixtest", "user/posixtest.elf"),
    ("libctest", "user/libctest.elf"),
    ("libc_so", "user/libc.so"),
    ("dynhello", "user/dynhello.elf"),
    ("dynposix", "user/dynposix.elf")
]

out = [
    "// Auto-generated ELF binary headers for LemonOS Userspace Programs",
    "#pragma once",
    "#include <stdint.h>",
    ""
]

for name, fpath in apps:
    if os.path.exists(fpath):
        data = open(fpath, "rb").read()
        out.append(f"static const uint32_t bin_{name}_len = {len(data)};")
        out.append(f"static const uint8_t bin_{name}_data[{len(data)}] __attribute__((aligned(16))) = {{")
        lines = [", ".join(f"0x{b:02x}" for b in data[i:i+16]) for i in range(0, len(data), 16)]
        out.append(",\n    ".join("    " + l for l in lines))
        out.append("};\n")

with open("include/user_elf_bins.h", "w") as f:
    f.write("\n".join(out))
