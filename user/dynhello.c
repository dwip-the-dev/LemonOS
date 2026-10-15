extern int puts(const char* s);
extern int getpid(void);
extern void exit(int status);

void _start(void) {
    puts("[Ring 3] Hello from Dynamically Linked Executable!");
    puts("[Ring 3] Function calls resolved via GOT & PLT Jump Slots!");
    exit(0);
}
