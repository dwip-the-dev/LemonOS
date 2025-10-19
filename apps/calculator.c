#include "calculator.h"
#include "vga.h"
#include "keyboard.h"
#include "vga_utils.h"

// Simple string to double conversion
double string_to_double(const char* str) {
    double result = 0.0;
    double fraction = 1.0;
    int sign = 1;
    int decimal_found = 0;
    
    // Handle sign
    if (*str == '-') {
        sign = -1;
        str++;
    } else if (*str == '+') {
        str++;
    }
    
    // Convert string to double
    while (*str) {
        if (*str == '.') {
            if (decimal_found) break;
            decimal_found = 1;
        } 
        else if (*str >= '0' && *str <= '9') {
            if (decimal_found) {
                fraction *= 0.1;
                result += (*str - '0') * fraction;
            } else {
                result = result * 10.0 + (*str - '0');
            }
        }
        else if (*str == ' ' || *str == '\t') {
            // Skip whitespace
        }
        else {
            break;
        }
        str++;
    }
    
    return result * sign;
}

// Simple double to string conversion for printing
void print_double(double num) {
    // Handle negative numbers
    if (num < 0) {
        vga_putchar('-');
        num = -num;
    }
    
    // Extract integer and fractional parts
    int int_part = (int)num;
    double frac_part = num - int_part;
    
    // Print integer part
    if (int_part == 0) {
        vga_putchar('0');
    } else {
        char int_buffer[16];
        int i = 0;
        
        while (int_part > 0) {
            int_buffer[i++] = '0' + (int_part % 10);
            int_part /= 10;
        }
        
        while (i > 0) {
            vga_putchar(int_buffer[--i]);
        }
    }
    
    // Print decimal point
    vga_putchar('.');
    
    // Print fractional part (2 decimal places)
    frac_part *= 100;
    int frac_int = (int)(frac_part + 0.5);
    
    if (frac_int > 99) frac_int = 99;
    
    if (frac_int < 10) {
        vga_putchar('0');
        vga_putchar('0' + frac_int);
    } else {
        vga_putchar('0' + (frac_int / 10));
        vga_putchar('0' + (frac_int % 10));
    }
}

// Get a single character input
char get_char_input(void) {
    vga_print("> ");
    
    while(1) {
        poll_keyboard();
        
        if (has_key()) {
            char c = getchar();
            
            if (c == '+' || c == '-' || c == '*' || c == '/') {
                vga_putchar(c);
                vga_println("");
                return c;
            }
            else if (c == '\b') {
                continue;
            }
            else if (c == '\n') {
                vga_println("");
                vga_print("Error: Please enter an operator (+, -, *, /)");
                vga_println("");
                vga_print("> ");
                continue;
            }
            else {
                vga_putchar(c);
                vga_print(" - Invalid operator! Use +, -, *, or /");
                vga_println("");
                vga_print("> ");
                continue;
            }
        }
    }
}

// Get a double value from user input
double get_double_input(void) {
    char buffer[32];
    int pos = 0;
    int decimal_points = 0;
    int has_digits = 0;
    
    vga_print("> ");
    
    while(1) {
        poll_keyboard();
        
        if (has_key()) {
            char c = getchar();
            
            if (c == '\n') {
                if (pos == 0 || !has_digits) {
                    vga_println("Error: Please enter a valid number");
                    vga_print("> ");
                    pos = 0;
                    decimal_points = 0;
                    has_digits = 0;
                    continue;
                }
                
                buffer[pos] = '\0';
                vga_println("");
                return string_to_double(buffer);
            }
            else if (c == '\b') {
                if (pos > 0) {
                    pos--;
                    if (buffer[pos] == '.') decimal_points--;
                    vga_backspace();
                }
                continue;
            }
            else if (c >= '0' && c <= '9') {
                if (pos < 31) {
                    buffer[pos++] = c;
                    vga_putchar(c);
                    has_digits = 1;
                }
            }
            else if (c == '.' && decimal_points == 0) {
                if (pos < 31) {
                    buffer[pos++] = c;
                    vga_putchar(c);
                    decimal_points = 1;
                }
            }
            else if (c == '-' && pos == 0) {
                buffer[pos++] = c;
                vga_putchar(c);
            }
        }
    }
}

// Calculator command implementation
void calc_command(void) {
    char op;
    double a, b, res;
    
    vga_println("=== LemonOS Calculator ===");
    vga_println("Available operators: +, -, *, /");
    vga_println("");
    
    op = get_char_input();
    
    vga_print("Enter first number: ");
    a = get_double_input();
    
    vga_print("Enter second number: ");
    b = get_double_input();
    
    vga_println("");
    vga_print("Calculating: ");
    print_double(a);
    vga_putchar(' ');
    vga_putchar(op);
    vga_putchar(' ');
    print_double(b);
    vga_println("");
    
    if (op == '+') {
        res = a + b;
        vga_print("Result: ");
        print_double(res);
        vga_println("");
    } 
    else if (op == '-') {
        res = a - b;
        vga_print("Result: ");
        print_double(res);
        vga_println("");
    } 
    else if (op == '*') {
        res = a * b;
        vga_print("Result: ");
        print_double(res);
        vga_println("");
    } 
    else if (op == '/') {
        if (b != 0.0) {
            res = a / b;
            vga_print("Result: ");
            print_double(res);
            vga_println("");
        } else {
            vga_println("Error: Division by zero!");
        }
    }
    
    vga_println("=== Calculation Complete ===");
}
