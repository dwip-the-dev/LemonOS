#include <stdio.h>

int main(int argc, char** argv) {
    if (argc <= 1) {
        printf("Usage: echo <text...>\n");
        return 0;
    }
    for (int i = 1; i < argc; i++) {
        if (argv[i]) {
            printf("%s%s", argv[i], (i < argc - 1) ? " " : "");
        }
    }
    printf("\n");
    return 0;
}
