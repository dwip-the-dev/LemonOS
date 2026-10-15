#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv) {
    const char* path = "/docs/welcome.txt";
    if (argc > 1 && argv[1] && argv[1][0] != '\0') {
        path = argv[1];
    }

    FILE* fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "cat: Cannot open '%s'\n", path);
        return 1;
    }

    char buf[256];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf) - 1, fp)) > 0) {
        buf[n] = '\0';
        fputs(buf, stdout);
    }

    fclose(fp);
    return 0;
}
