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

// Simple double to string conversion for printing with colors
void print_double(double num) {
    // Handle negative numbers
    if (num < 0) {
        vga_putchar_color('-', VGA_COLOR_RED, VGA_COLOR_BLACK);
        num = -num;
    } else {
        vga_putchar_color('+', VGA_COLOR_GREEN, VGA_COLOR_BLACK);
    }
    
    // Extract integer and fractional parts
    int int_part = (int)num;
    double frac_part = num - int_part;
    
    // Print integer part in cyan
    if (int_part == 0) {
        vga_putchar_color('0', VGA_COLOR_CYAN, VGA_COLOR_BLACK);
    } else {
        char int_buffer[16];
        int i = 0;
        
        while (int_part > 0) {
            int_buffer[i++] = '0' + (int_part % 10);
            int_part /= 10;
        }
        
        while (i > 0) {
            vga_putchar_color(int_buffer[--i], VGA_COLOR_CYAN, VGA_COLOR_BLACK);
        }
    }
    
    // Print decimal point in white
    vga_putchar_color('.', VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    
    // Print fractional part (2 decimal places) in light blue
    frac_part *= 100;
    int frac_int = (int)(frac_part + 0.5);
    
    if (frac_int > 99) frac_int = 99;
    
    if (frac_int < 10) {
        vga_putchar_color('0', VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK);
        vga_putchar_color('0' + frac_int, VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK);
    } else {
        vga_putchar_color('0' + (frac_int / 10), VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK);
        vga_putchar_color('0' + (frac_int % 10), VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK);
    }
}

// Get a single character input with colors
char get_char_input(void) {
    vga_print_color("> ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    
    while(1) {
        poll_keyboard();
        
        if (has_key()) {
            char c = getchar();
            
            if (c == '+' || c == '-' || c == '*' || c == '/') {
                vga_putchar_color(c, VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
                vga_println("");
                return c;
            }
            else if (c == '\b') {
                continue;
            }
            else if (c == '\n') {
                vga_println("");
                vga_print_color("Error: Please enter an operator (+, -, *, /)", VGA_COLOR_RED, VGA_COLOR_BLACK);
                vga_println("");
                vga_print_color("> ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
                continue;
            }
            else {
                vga_putchar_color(c, VGA_COLOR_RED, VGA_COLOR_BLACK);
                vga_print_color(" - Invalid operator! Use +, -, *, or /", VGA_COLOR_RED, VGA_COLOR_BLACK);
                vga_println("");
                vga_print_color("> ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
                continue;
            }
        }
    }
}

// Get a double value from user input with colors
double get_double_input(void) {
    char buffer[32];
    int pos = 0;
    int decimal_points = 0;
    int has_digits = 0;
    
    vga_print_color("> ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    
    while(1) {
        poll_keyboard();
        
        if (has_key()) {
            char c = getchar();
            
            if (c == '\n') {
                if (pos == 0 || !has_digits) {
                    vga_println_color("Error: Please enter a valid number", VGA_COLOR_RED, VGA_COLOR_BLACK);
                    vga_print_color("> ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
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
                    vga_putchar_color(c, VGA_COLOR_CYAN, VGA_COLOR_BLACK);
                    has_digits = 1;
                }
            }
            else if (c == '.' && decimal_points == 0) {
                if (pos < 31) {
                    buffer[pos++] = c;
                    vga_putchar_color(c, VGA_COLOR_WHITE, VGA_COLOR_BLACK);
                    decimal_points = 1;
                }
            }
            else if (c == '-' && pos == 0) {
                buffer[pos++] = c;
                vga_putchar_color(c, VGA_COLOR_RED, VGA_COLOR_BLACK);
            }
            else {
                // Invalid character - show error but don't add to buffer
                vga_putchar_color(c, VGA_COLOR_RED, VGA_COLOR_BLACK);
                vga_backspace(); // Remove the invalid character
                vga_print_color("?", VGA_COLOR_RED, VGA_COLOR_BLACK);
                vga_backspace(); // Remove the question mark
            }
        }
    }
}

// Print operator with appropriate color
void print_operator(char op) {
    switch(op) {
        case '+':
            vga_putchar_color('+', VGA_COLOR_GREEN, VGA_COLOR_BLACK);
            break;
        case '-':
            vga_putchar_color('-', VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
            break;
        case '*':
            vga_putchar_color('*', VGA_COLOR_CYAN, VGA_COLOR_BLACK);
            break;
        case '/':
            vga_putchar_color('/', VGA_COLOR_MAGENTA, VGA_COLOR_BLACK);
            break;
        default:
            vga_putchar_color(op, VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    }
}

// Calculator command implementation with colors
void calc_command(void) {
    char op;
    double a, b, res;
    
    vga_clear();
    vga_println_color("=== LemonOS Calculator ===", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    vga_println_color("Available operators: +, -, *, /", VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    vga_println("");
    
    vga_print_color("Select operator ", VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    vga_print_color("(+, -, *, /)", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    vga_println_color(":", VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    op = get_char_input();
    
    vga_print_color("Enter first number: ", VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    a = get_double_input();
    
    vga_print_color("Enter second number: ", VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    b = get_double_input();
    
    vga_println("");
    vga_print_color("Calculating: ", VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    print_double(a);
    vga_putchar_color(' ', VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    print_operator(op);
    vga_putchar_color(' ', VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    print_double(b);
    vga_println("");
    
    vga_print_color("Result: ", VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    
    if (op == '+') {
        res = a + b;
        print_double(res);
        vga_println("");
    } 
    else if (op == '-') {
        res = a - b;
        print_double(res);
        vga_println("");
    } 
    else if (op == '*') {
        res = a * b;
        print_double(res);
        vga_println("");
    } 
    else if (op == '/') {
        if (b != 0.0) {
            res = a / b;
            print_double(res);
            vga_println("");
        } else {
            vga_println_color("Error: Division by zero!", VGA_COLOR_RED, VGA_COLOR_BLACK);
        }
    }
    
    vga_println("");
    vga_println_color("=== Calculation Complete ===", VGA_COLOR_GREEN, VGA_COLOR_BLACK);
    vga_println("");
    
    // Reset to default colors
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
}
