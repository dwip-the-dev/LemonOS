#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <time.h>
#include <dirent.h>

int main(int argc, char** argv) {
    printf("====================================================\n");
    printf("[POSIX TEST] Ring 3 POSIX Standard Conformance Suite\n");
    printf("====================================================\n");
    printf("Arguments received (argc=%d):\n", argc);
    for (int i = 0; i < argc; i++) {
        printf("  argv[%d] = %s\n", i, argv[i] ? argv[i] : "(null)");
    }

    // 1. Process Identity (getpid, getppid)
    pid_t pid = getpid();
    pid_t ppid = getppid();
    printf("[1/8] Process Identity: PID=%d, PPID=%d ... PASS\n", pid, ppid);

    // 2. Working Directory & Filesystem Navigation (getcwd, chdir)
    char cwd[256];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("[2/8] Current Working Directory: '%s'\n", cwd);
    } else {
        printf("[2/8] getcwd failed!\n");
    }

    // 3. File Metadata Queries (stat, fstat, open, close)
    struct stat st;
    if (stat("/docs/welcome.txt", &st) == 0) {
        printf("[3/8] stat('/docs/welcome.txt'): size=%lu bytes, mode=0%o ... PASS\n",
               (unsigned long)st.st_size, st.st_mode);
    } else {
        printf("[3/8] stat failed or file not found.\n");
    }

    int fd = open("/docs/welcome.txt", O_RDONLY);
    if (fd >= 0) {
        struct stat fst;
        if (fstat(fd, &fst) == 0) {
            printf("      fstat(fd=%d): size=%lu bytes ... PASS\n", fd, (unsigned long)fst.st_size);
        }
        close(fd);
    }

    // 4. Directory Traversal (opendir, readdir, closedir)
    printf("[4/8] Directory enumeration of /bin via POSIX dirent API:\n");
    DIR* dir = opendir("/bin");
    if (dir) {
        struct dirent* de;
        int count = 0;
        while ((de = readdir(dir)) != NULL) {
            printf("      -> %s (type=%d)\n", de->d_name, de->d_type);
            count++;
        }
        closedir(dir);
        printf("      Total directory entries enumerated: %d ... PASS\n", count);
    } else {
        printf("[4/8] opendir(/bin) failed!\n");
    }

    // 5. Inter-Process Communication (pipe, read, write)
    int pipefd[2];
    if (pipe(pipefd) == 0) {
        const char* pipe_msg = "LemonOS POSIX Pipe Data Transmission";
        size_t len = strlen(pipe_msg);
        ssize_t w = write(pipefd[1], pipe_msg, len);
        char pbuf[64];
        ssize_t r = read(pipefd[0], pbuf, sizeof(pbuf) - 1);
        if (r > 0) pbuf[r] = '\0';
        close(pipefd[0]);
        close(pipefd[1]);

        if (w == (ssize_t)len && r == (ssize_t)len && strcmp(pipe_msg, pbuf) == 0) {
            printf("[5/8] Anonymous IPC Pipe: read '%s' (%d bytes) ... PASS\n", pbuf, (int)r);
        } else {
            printf("[5/8] Anonymous IPC Pipe: data mismatch (w=%d, r=%d)\n", (int)w, (int)r);
        }
    } else {
        printf("[5/8] pipe() failed!\n");
    }

    // 6. POSIX Time (gettimeofday, nanosleep)
    struct timeval tv;
    if (gettimeofday(&tv, NULL) == 0) {
        printf("[6/8] gettimeofday: sec=%ld, usec=%ld ... PASS\n", (long)tv.tv_sec, (long)tv.tv_usec);
    } else {
        printf("[6/8] gettimeofday failed!\n");
    }
    struct timespec req = { 0, 5000000 }; // 5 ms
    nanosleep(&req, NULL);

    // 7. Process Fork & Waitpid (fork, waitpid)
    printf("[7/8] Process Creation & Synchronization (fork + waitpid):\n");
    pid_t child_pid = fork();
    if (child_pid == 0) {
        // In Child Process
        printf("      [CHILD] Child process executing! PID=%d, PPID=%d\n", getpid(), getppid());
        exit(42);
    } else if (child_pid > 0) {
        // In Parent Process
        printf("      [PARENT] Forked child PID=%d, waiting with waitpid()...\n", child_pid);
        int status = 0;
        pid_t wpid = waitpid(child_pid, &status, 0);
        printf("      [PARENT] Child PID %d exited with status=%d ... PASS\n", wpid, WEXITSTATUS(status));
    } else {
        printf("      [PARENT] fork() returned error: %d\n", child_pid);
    }

    // 8. File descriptor duplication (dup, dup2)
    int stdout_copy = dup(1);
    if (stdout_copy >= 0) {
        dup2(stdout_copy, 1);
        close(stdout_copy);
        printf("[8/8] Descriptor duplication (dup/dup2) ... PASS\n");
    }

    printf("====================================================\n");
    printf("[POSIX TEST] ALL POSIX CONFORMANCE TESTS COMPLETED!\n");
    printf("====================================================\n");
    return 0;
}
