#include "file_manager.h"
#include "vga.h"
#include "keyboard.h"
#include "filesystem.h"

// Simple string functions
int simple_strcmp(const char* a, const char* b) {
    while (*a && (*a == *b)) { a++; b++; }
    return *(unsigned char*)a - *(unsigned char*)b;
}

void simple_strcpy(char* dest, const char* src) {
    while (*src) *dest++ = *src++;
    *dest = '\0';
}

int starts_with(const char* str, const char* prefix) {
    while (*prefix) {
        if (*prefix++ != *str++) return 0;
    }
    return 1;
}

void extract_filename(const char* command, char* filename) {
    const char* ptr = command;
    while (*ptr && *ptr != ' ') ptr++;
    while (*ptr && *ptr == ' ') ptr++;
    
    int i = 0;
    while (*ptr && *ptr != ' ' && i < 31) {
        filename[i++] = *ptr++;
    }
    filename[i] = '\0';
}

// Simple number to string conversion
void print_number(int num) {
    if (num == 0) {
        vga_putchar('0');
        return;
    }
    
    char buffer[16];
    int i = 0;
    
    while (num > 0) {
        buffer[i++] = '0' + (num % 10);
        num /= 10;
    }
    
    while (i > 0) {
        vga_putchar(buffer[--i]);
    }
}

// Simple text editor
void simple_editor(const char* filename) {
    char content[1024] = {0};
    char* existing_content = fs_read_file(filename);
    
    if (existing_content) {
        simple_strcpy(content, existing_content);
    }
    
    int content_len = 0;
    while (content[content_len]) content_len++;
    
    vga_clear();
    vga_print("=== Editing: ");
    vga_print(filename);
    vga_println(" ===");
    vga_println("Type your text. Press F1 to finish.");
    vga_println("Current content:");
    vga_println("----------------");
    vga_println(content);
    vga_println("----------------");
    vga_print("New content: ");
    
    // Simple input loop
    int pos = 0;
    char input[1024] = {0};
    
    while (1) {
        poll_keyboard();
        if (has_key()) {
            char c = getchar();
            
            if (c == 0x01) { // F1 - Save
                break;
            }
            else if (c == '\n') {
                // Add newline to content
                if (pos < 1023) {
                    input[pos++] = '\n';
                    vga_putchar('\n');
                    vga_print("> ");
                }
            }
            else if (c == '\b') {
                // Backspace
                if (pos > 0) {
                    pos--;
                    vga_putchar('\b');
                    vga_putchar(' ');
                    vga_putchar('\b');
                }
            }
            else if (c >= 32 && c <= 126 && pos < 1023) {
                // Regular character
                input[pos++] = c;
                vga_putchar(c);
            }
        }
    }
    
    // Save the file
    if (pos > 0) {
        input[pos] = '\0';
        fs_write_file(filename, input);
        vga_println("\nFile saved successfully!");
    } else {
        vga_println("\nNo changes made.");
    }
}

void file_manager_command(void) {
    fs_init();
    
    vga_clear();
    vga_println("=== LEMONOS FILE MANAGER ===");
    vga_println("Type 'help' for commands");
    vga_println("");
    
    char input[64];
    int pos = 0;
    
    while (1) {
        vga_print("file> ");
        pos = 0;
        
        // Get input
        while (1) {
            poll_keyboard();
            if (has_key()) {
                char c = getchar();
                
                if (c == '\n') {
                    input[pos] = '\0';
                    vga_putchar('\n');
                    break;
                } 
                else if (c == '\b') {
                    if (pos > 0) {
                        pos--;
                        vga_putchar('\b');
                        vga_putchar(' ');
                        vga_putchar('\b');
                    }
                }
                else if (c >= 32 && c <= 126 && pos < 63) {
                    input[pos++] = c;
                    vga_putchar(c);
                }
            }
        }
        
        // Process command
        if (simple_strcmp(input, "exit") == 0) {
            break;
        } 
        else if (simple_strcmp(input, "list") == 0) {
            fs_list_files();
        } 
        else if (simple_strcmp(input, "help") == 0) {
            vga_println("Commands:");
            vga_println("  list                    - Show all files");
            vga_println("  view <filename>         - View file content");
            vga_println("  edit <filename>         - Edit file (Type text, press F1 to save)");
            vga_println("  create <filename>       - Create new file");
            vga_println("  delete <filename>       - Delete file");
            vga_println("  exit                    - Return to main menu");
        }
        else if (starts_with(input, "view ")) {
            char filename[32];
            extract_filename(input, filename);
            
            if (fs_file_exists(filename)) {
                char* content = fs_read_file(filename);
                vga_print("=== ");
                vga_print(filename);
                vga_println(" ===");
                vga_println(content);
            } else {
                vga_print("File '");
                vga_print(filename);
                vga_println("' not found!");
            }
        }
        else if (starts_with(input, "edit ")) {
            char filename[32];
            extract_filename(input, filename);
            
            if (fs_file_exists(filename)) {
                simple_editor(filename);
            } else {
                vga_print("File '");
                vga_print(filename);
                vga_println("' not found! Use 'create' to make a new file.");
            }
        }
        else if (starts_with(input, "create ")) {
            char filename[32];
            extract_filename(input, filename);
            
            if (fs_create_file(filename, "New file created.\n") == 0) {
                vga_print("File '");
                vga_print(filename);
                vga_println("' created successfully!");
            } else {
                vga_print("Error creating '");
                vga_print(filename);
                vga_println("' - file may already exist or no space!");
            }
        }
        else if (starts_with(input, "delete ")) {
            char filename[32];
            extract_filename(input, filename);
            
            if (fs_file_exists(filename)) {
                vga_print("Delete '");
                vga_print(filename);
                vga_print("'? (y/n): ");
                
                // Get confirmation
                while (1) {
                    poll_keyboard();
                    if (has_key()) {
                        char confirm = getchar();
                        vga_putchar(confirm);
                        vga_println("");
                        
                        if (confirm == 'y' || confirm == 'Y') {
                            if (fs_delete_file(filename) == 0) {
                                vga_println("File deleted.");
                            } else {
                                vga_println("Error deleting file.");
                            }
                        } else {
                            vga_println("Deletion cancelled.");
                        }
                        break;
                    }
                }
            } else {
                vga_print("File '");
                vga_print(filename);
                vga_println("' not found!");
            }
        }
        else {
            vga_print("Unknown command: ");
            vga_println(input);
            vga_println("Type 'help' for available commands.");
        }
        
        vga_println("");
    }
    
    vga_clear();
    vga_println("Returned to main menu.");
}
