# LemonOS POSIX Subsystem & Standard C Library (libc)

**Author:** Dwip Biswas ([@dwip-the-dev](https://github.com/dwip-the-dev))  
**Target:** i386-lemon-elf (System V i386 ABI)  
**Standard:** IEEE Std 1003.1 (POSIX) & ISO/IEC 9899:1999 (C99)  

---

## 1. System Call Interface

LemonOS exposes POSIX services via software interrupt `int 0x80`. The system call number is passed in `EAX`, with arguments in `EBX`, `ECX`, `EDX`, `ESI`, and `EDI`. The return value is returned in `EAX` (negative values indicate negative `errno` codes).

### Implemented System Calls

| Syscall # | Name | Description | Conformance |
|-----------|------|-------------|-------------|
| **1** | `SYS_EXIT` | Terminates process with exit status | POSIX.1-2008 |
| **2** | `SYS_FORK` | Duplicates calling process | POSIX.1-2008 |
| **3** | `SYS_READ` | Reads from file descriptor / device / pipe | POSIX.1-2008 |
| **4** | `SYS_WRITE` | Writes to file descriptor / device / pipe | POSIX.1-2008 |
| **5** | `SYS_OPEN` | Opens a file with POSIX flags (`O_RDONLY`, `O_CREAT`, etc.) | POSIX.1-2008 |
| **6** | `SYS_CLOSE` | Closes an open file descriptor | POSIX.1-2008 |
| **7** | `SYS_WAITPID` | Waits for child process state changes | POSIX.1-2008 |
| **10** | `SYS_UNLINK` | Removes a directory entry | POSIX.1-2008 |
| **11** | `SYS_EXECVE` | Executes an ELF32 binary | POSIX.1-2008 |
| **12** | `SYS_CHDIR` | Changes process working directory | POSIX.1-2008 |
| **18** | `SYS_STAT` | Obtains file status by pathname | POSIX.1-2008 |
| **19** | `SYS_LSEEK` | Repositions read/write file offset | POSIX.1-2008 |
| **20** | `SYS_GETPID` | Returns process ID | POSIX.1-2008 |
| **28** | `SYS_FSTAT` | Obtains file status by open file descriptor | POSIX.1-2008 |
| **41** | `SYS_DUP` | Duplicates an open file descriptor | POSIX.1-2008 |
| **42** | `SYS_PIPE` | Creates an unidirectional interprocess pipe | POSIX.1-2008 |
| **45** | `SYS_BRK` | Changes data segment size for heap allocation | POSIX / Linux |
| **54** | `SYS_IOCTL` | Device control (`TCGETS`, `TCSETS`, `TCSETSW`, `TCSETSF`) | POSIX.1-2008 |
| **55** | `SYS_FCNTL` | File control (`F_DUPFD`, `F_GETFD`, `F_SETFD`, `F_GETFL`, `F_SETFL`) | POSIX.1-2008 |
| **63** | `SYS_DUP2` | Duplicates descriptor to specific target descriptor | POSIX.1-2008 |
| **64** | `SYS_GETPPID`| Returns parent process ID | POSIX.1-2008 |
| **78** | `SYS_GETTIMEOFDAY` | Retrieves time in seconds and microseconds | POSIX.1-2001 |
| **141**| `SYS_GETDENTS` | Enumerates directory entries into `dirent` structures | POSIX / Linux |
| **183**| `SYS_GETCWD` | Copies absolute current working directory to buffer | POSIX.1-2008 |
| **228**| `SYS_CLOCK_GETTIME` | Returns monotonic/realtime clock with nanosecond precision | POSIX.1-2008 |

---

## 2. Standard C Library (`libc/`)

The LemonOS userspace C library is built from source and provides standard C headers:

### `<stdio.h>`
- **Stream Operations:** `fopen`, `fclose`, `fread`, `fwrite`, `fseek`, `ftell`, `rewind`, `feof`, `ferror`, `clearerr`.
- **Formatted I/O:** `printf`, `fprintf`, `sprintf`, `snprintf`, `vprintf`, `vfprintf`, `vsprintf`, `vsnprintf`.
- **Character I/O:** `fgetc`, `fputc`, `getchar`, `putchar`, `fgets`, `fputs`, `puts`.
- **File Management:** `remove`, `rename`.

### `<stdlib.h>`
- **Dynamic Memory:** `malloc()`, `calloc()`, `realloc()`, `free()` backed by `brk` / `sbrk`.
- **Searching & Sorting:** `qsort()` (in-place quicksort), `bsearch()` (binary search).
- **String Conversion:** `atoi()`, `atol()`, `strtol()`, `strtoul()`.
- **Environment & Process:** `getenv()`, `setenv()`, `exit()`, `abort()`.

### `<string.h>`
- `strlen`, `strcpy`, `strncpy`, `strcat`, `strncat`, `strcmp`, `strncmp`, `strchr`, `strrchr`, `strstr`, `strtok`, `strdup`, `memset`, `memcpy`, `memmove`, `memcmp`, `memchr`, `strerror`.

### `<math.h>`
- `sqrt()`, `pow()`, `sin()`, `cos()`, `tan()`, `floor()`, `ceil()`, `fabs()`, `fmod()`.

### `<setjmp.h>`
- `setjmp()`, `longjmp()` saving and restoring callee-saved registers (`EBX`, `ESI`, `EDI`, `EBP`, `ESP`, and return address).

### `<termios.h>` & `<unistd.h>`
- `tcgetattr()`, `tcsetattr()` querying and modifying terminal modes (`ICANON`, `ECHO`, baud rates).
- `isatty(fd)` detecting whether a file descriptor connects to an interactive terminal device.

### `<time.h>`
- `time()`, `posix_mktime()`, `gmtime_r()`, `localtime_r()`, `clock_gettime()`.
- `strftime()` formatting dates and timestamps with full specifier expansion (`%Y`, `%m`, `%d`, `%H`, `%M`, `%S`, `%F`, `%T`, `%a`, `%b`, `%Z`).

---

## 3. Dynamic Linker & Shared Libraries (`drivers/dynlink.c`)

LemonOS features an ELF dynamic loader and runtime linker:

1. **Relocatable Modules:** Compiles `libc.so` as position-independent code (PIC) with dynamic symbol tables (`.dynsym`), string tables (`.dynstr`), and hash tables (`.hash`).
2. **Procedure Linkage Table (PLT) & Global Offset Table (GOT):**
   - Resolves `R_386_JMP_SLOT` relocations during executable launch or on-demand.
   - Binds external symbol references from user executables (`dynposix`, `dynhello`) to `libc.so`.
3. **Base Address Relocation:**
   - Evaluates `R_386_RELATIVE` offsets when loading shared libraries at arbitrary virtual address bases (e.g. `0x00A00000`).
4. **Verification:**
   - Validated via `/bin/dynposix` running in Ring 3 userspace, binding to `libc.so`, resolving `getpid()`, and performing dynamic heap allocations.
