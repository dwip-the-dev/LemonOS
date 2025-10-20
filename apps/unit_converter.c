#include "unit_converter.h"
#include "vga.h"
#include "keyboard.h"
#include "vga_utils.h"

// Helper to get numeric input.
static double get_numeric_input(const char* prompt) {
    vga_print(prompt);
    
    char input[32];
    int pos = 0;
    
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
            else if ((c >= '0' && c <= '9') || c == '.' || c == '-') {
                if (pos < 31) {
                    input[pos++] = c;
                    vga_putchar(c);
                }
            }
        }
    }
    
    // Simple string to double conversion
    double result = 0.0;
    double fraction = 1.0;
    int is_negative = 0;
    int has_decimal = 0;
    
    for (int i = 0; i < pos; i++) {
        if (input[i] == '-') {
            is_negative = 1;
        }
        else if (input[i] == '.') {
            has_decimal = 1;
        }
        else if (input[i] >= '0' && input[i] <= '9') {
            if (has_decimal) {
                fraction *= 0.1;
                result += (input[i] - '0') * fraction;
            } else {
                result = result * 10 + (input[i] - '0');
            }
        }
    }
    
    return is_negative ? -result : result;
}

// Helper to print the result
static void print_result(double input, const char* from, double result, const char* to) {
    vga_print_color(from, VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_print_color(" -> ", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_print_color(to, VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_println_color("", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    
    // Manual printf implementation
    vga_print("Result: ");
    
    // Print input value
    if (input < 0) {
        vga_putchar('-');
        input = -input;
    }
    
    int integer_part = (int)input;
    double fractional_part = input - integer_part;
    
    // Print integer part
    if (integer_part == 0) {
        vga_putchar('0');
    } else {
        char int_str[20];
        int int_pos = 0;
        int temp = integer_part;
        
        while (temp > 0) {
            int_str[int_pos++] = '0' + (temp % 10);
            temp /= 10;
        }
        while (int_pos > 0) {
            vga_putchar(int_str[--int_pos]);
        }
    }
    
    // Print fractional part (2 decimal places).
    vga_putchar('.');
    fractional_part *= 100;
    int frac_part = (int)fractional_part;
    
    if (frac_part < 10) {
        vga_putchar('0');
    }
    
    if (frac_part == 0) {
        vga_putchar('0');
    } else {
        char frac_str[3];
        int frac_pos = 0;
        int temp = frac_part;
        
        while (temp > 0) {
            frac_str[frac_pos++] = '0' + (temp % 10);
            temp /= 10;
        }
        while (frac_pos > 0) {
            vga_putchar(frac_str[--frac_pos]);
        }
    }
    
    vga_print(" ");
    vga_print(from);
    vga_print(" = ");
    
    // Print result value
    if (result < 0) {
        vga_putchar('-');
        result = -result;
    }
    
    integer_part = (int)result;
    fractional_part = result - integer_part;
    
    // Print integer part
    if (integer_part == 0) {
        vga_putchar('0');
    } else {
        char int_str[20];
        int int_pos = 0;
        int temp = integer_part;
        
        while (temp > 0) {
            int_str[int_pos++] = '0' + (temp % 10);
            temp /= 10;
        }
        while (int_pos > 0) {
            vga_putchar(int_str[--int_pos]);
        }
    }
    
    // Print fractional part upto 2 decimal places.
    vga_putchar('.');
    fractional_part *= 100;
    frac_part = (int)fractional_part;
    
    if (frac_part < 10) {
        vga_putchar('0');
    }
    
    if (frac_part == 0) {
        vga_putchar('0');
    } else {
        char frac_str[3];
        int frac_pos = 0;
        int temp = frac_part;
        
        while (temp > 0) {
            frac_str[frac_pos++] = '0' + (temp % 10);
            temp /= 10;
        }
        while (frac_pos > 0) {
            vga_putchar(frac_str[--frac_pos]);
        }
    }
    
    vga_print(" ");
    vga_println(to);
}

// Temp conversions
static double celsius_to_fahrenheit(double c) { return (c * 9/5) + 32; }
static double fahrenheit_to_celsius(double f) { return (f - 32) * 5/9; }
static double celsius_to_kelvin(double c) { return c + 273.15; }
static double kelvin_to_celsius(double k) { return k - 273.15; }
static double fahrenheit_to_kelvin(double f) { return (f - 32) * 5/9 + 273.15; }
static double kelvin_to_fahrenheit(double k) { return (k - 273.15) * 9/5 + 32; }

// Distance conversions
static double km_to_miles(double km) { return km * 0.621371; }
static double miles_to_km(double mi) { return mi / 0.621371; }
static double meters_to_feet(double m) { return m * 3.28084; }
static double feet_to_meters(double ft) { return ft / 3.28084; }
static double cm_to_inches(double cm) { return cm * 0.393701; }
static double inches_to_cm(double in) { return in / 0.393701; }

// Weight conversions
static double kg_to_lbs(double kg) { return kg * 2.20462; }
static double lbs_to_kg(double lbs) { return lbs / 2.20462; }
static double kg_to_grams(double kg) { return kg * 1000; }
static double grams_to_kg(double g) { return g / 1000; }
static double kg_to_ounces(double kg) { return kg * 35.274; }
static double ounces_to_kg(double oz) { return oz / 35.274; }

// Volume conversions
static double liters_to_gallons(double l) { return l * 0.264172; }
static double gallons_to_liters(double gal) { return gal / 0.264172; }
static double liters_to_ml(double l) { return l * 1000; }
static double ml_to_liters(double ml) { return ml / 1000; }
static double liters_to_cups(double l) { return l * 4.22675; }
static double cups_to_liters(double cups) { return cups / 4.22675; }

// Digital conversions
static double mb_to_gb(double mb) { return mb / 1024; }
static double gb_to_mb(double gb) { return gb * 1024; }
static double gb_to_tb(double gb) { return gb / 1024; }
static double tb_to_gb(double tb) { return tb * 1024; }
static double kb_to_mb(double kb) { return kb / 1024; }
static double mb_to_kb(double mb) { return mb * 1024; }

// NEW: Sub-menu functions for each conversion type
static void temperature_submenu(void) {
    vga_clear();
    vga_println_color("=== TEMPERATURE CONVERSIONS ===", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    vga_println_color("1. Celsius to Fahrenheit", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_println_color("2. Fahrenheit to Celsius", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_println_color("3. Celsius to Kelvin", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_println_color("4. Kelvin to Celsius", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_println_color("5. Fahrenheit to Kelvin", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_println_color("6. Kelvin to Fahrenheit", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_println_color("7. Back to Main Menu", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_println("");

    int choice = get_numeric_input("Choose conversion: ");
    
    if (choice == 7) return;
    
    double input = get_numeric_input("Enter temperature: ");
    double result = 0.0;
    const char* from = "";
    const char* to = "";

    switch(choice) {
        case 1: result = celsius_to_fahrenheit(input); from="C"; to="F"; break;
        case 2: result = fahrenheit_to_celsius(input); from="F"; to="C"; break;
        case 3: result = celsius_to_kelvin(input); from="C"; to="K"; break;
        case 4: result = kelvin_to_celsius(input); from="K"; to="C"; break;
        case 5: result = fahrenheit_to_kelvin(input); from="F"; to="K"; break;
        case 6: result = kelvin_to_fahrenheit(input); from="K"; to="F"; break;
        default: 
            vga_println_color("Invalid choice", VGA_COLOR_RED, VGA_COLOR_BLACK);
            return;
    }
    
    print_result(input, from, result, to);
}

static void distance_submenu(void) {
    vga_clear();
    vga_println_color("=== DISTANCE CONVERSIONS ===", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    vga_println_color("1. Kilometers to Miles", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_println_color("2. Miles to Kilometers", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_println_color("3. Meters to Feet", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_println_color("4. Feet to Meters", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_println_color("5. Centimeters to Inches", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_println_color("6. Inches to Centimeters", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_println_color("7. Back to Main Menu", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_println("");

    int choice = get_numeric_input("Choose conversion: ");
    
    if (choice == 7) return;
    
    double input = get_numeric_input("Enter distance: ");
    double result = 0.0;
    const char* from = "";
    const char* to = "";

    switch(choice) {
        case 1: result = km_to_miles(input); from="km"; to="miles"; break;
        case 2: result = miles_to_km(input); from="miles"; to="km"; break;
        case 3: result = meters_to_feet(input); from="m"; to="ft"; break;
        case 4: result = feet_to_meters(input); from="ft"; to="m"; break;
        case 5: result = cm_to_inches(input); from="cm"; to="inches"; break;
        case 6: result = inches_to_cm(input); from="inches"; to="cm"; break;
        default: 
            vga_println_color("Invalid choice", VGA_COLOR_RED, VGA_COLOR_BLACK);
            return;
    }
    
    print_result(input, from, result, to);
}

static void weight_submenu(void) {
    vga_clear();
    vga_println_color("=== WEIGHT CONVERSIONS ===", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    vga_println_color("1. Kilograms to Pounds", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_println_color("2. Pounds to Kilograms", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_println_color("3. Kilograms to Grams", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_println_color("4. Grams to Kilograms", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_println_color("5. Kilograms to Ounces", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_println_color("6. Ounces to Kilograms", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_println_color("7. Back to Main Menu", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_println("");

    int choice = get_numeric_input("Choose conversion: ");
    
    if (choice == 7) return;
    
    double input = get_numeric_input("Enter weight: ");
    double result = 0.0;
    const char* from = "";
    const char* to = "";

    switch(choice) {
        case 1: result = kg_to_lbs(input); from="kg"; to="lbs"; break;
        case 2: result = lbs_to_kg(input); from="lbs"; to="kg"; break;
        case 3: result = kg_to_grams(input); from="kg"; to="g"; break;
        case 4: result = grams_to_kg(input); from="g"; to="kg"; break;
        case 5: result = kg_to_ounces(input); from="kg"; to="oz"; break;
        case 6: result = ounces_to_kg(input); from="oz"; to="kg"; break;
        default: 
            vga_println_color("Invalid choice", VGA_COLOR_RED, VGA_COLOR_BLACK);
            return;
    }
    
    print_result(input, from, result, to);
}

static void volume_submenu(void) {
    vga_clear();
    vga_println_color("=== VOLUME CONVERSIONS ===", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    vga_println_color("1. Liters to Gallons", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_println_color("2. Gallons to Liters", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_println_color("3. Liters to Milliliters", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_println_color("4. Milliliters to Liters", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_println_color("5. Liters to Cups", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_println_color("6. Cups to Liters", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_println_color("7. Back to Main Menu", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_println("");

    int choice = get_numeric_input("Choose conversion: ");
    
    if (choice == 7) return;
    
    double input = get_numeric_input("Enter volume: ");
    double result = 0.0;
    const char* from = "";
    const char* to = "";

    switch(choice) {
        case 1: result = liters_to_gallons(input); from="L"; to="gal"; break;
        case 2: result = gallons_to_liters(input); from="gal"; to="L"; break;
        case 3: result = liters_to_ml(input); from="L"; to="mL"; break;
        case 4: result = ml_to_liters(input); from="mL"; to="L"; break;
        case 5: result = liters_to_cups(input); from="L"; to="cups"; break;
        case 6: result = cups_to_liters(input); from="cups"; to="L"; break;
        default: 
            vga_println_color("Invalid choice", VGA_COLOR_RED, VGA_COLOR_BLACK);
            return;
    }
    
    print_result(input, from, result, to);
}

static void digital_submenu(void) {
    vga_clear();
    vga_println_color("=== DIGITAL SIZE CONVERSIONS ===", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    vga_println_color("1. Megabytes to Gigabytes", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_println_color("2. Gigabytes to Megabytes", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_println_color("3. Gigabytes to Terabytes", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_println_color("4. Terabytes to Gigabytes", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_println_color("5. Kilobytes to Megabytes", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_println_color("6. Megabytes to Kilobytes", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_println_color("7. Back to Main Menu", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_println("");

    int choice = get_numeric_input("Choose conversion: ");
    
    if (choice == 7) return;
    
    double input = get_numeric_input("Enter size: ");
    double result = 0.0;
    const char* from = "";
    const char* to = "";

    switch(choice) {
        case 1: result = mb_to_gb(input); from="MB"; to="GB"; break;
        case 2: result = gb_to_mb(input); from="GB"; to="MB"; break;
        case 3: result = gb_to_tb(input); from="GB"; to="TB"; break;
        case 4: result = tb_to_gb(input); from="TB"; to="GB"; break;
        case 5: result = kb_to_mb(input); from="KB"; to="MB"; break;
        case 6: result = mb_to_kb(input); from="MB"; to="KB"; break;
        default: 
            vga_println_color("Invalid choice", VGA_COLOR_RED, VGA_COLOR_BLACK);
            return;
    }
    
    print_result(input, from, result, to);
}

// Main command function
void unit_converter_command(void) {
    int choice;
    
    while(1) {
        vga_clear();
        vga_println_color("=== UNIT CONVERTER ===", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
        vga_println_color("1. Temperature", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        vga_println_color("2. Distance", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        vga_println_color("3. Weight", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        vga_println_color("4. Volume", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        vga_println_color("5. Digital Size", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        vga_println_color("6. Exit", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        vga_println("");

        choice = get_numeric_input("Choose conversion type: ");
        
        switch(choice) {
            case 1: temperature_submenu(); break;
            case 2: distance_submenu(); break;
            case 3: weight_submenu(); break;
            case 4: volume_submenu(); break;
            case 5: digital_submenu(); break;
            case 6: return;
            default: 
                vga_println_color("Invalid choice", VGA_COLOR_RED, VGA_COLOR_BLACK);
                break;
        }
        
        vga_println("");
        vga_println_color("Press any key to continue...", VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        while (!has_key()) poll_keyboard();
        getchar();
    }
}
