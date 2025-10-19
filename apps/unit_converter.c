#include "unit_converter.h"
#include "vga.h"
#include "keyboard.h"
#include "vga_utils.h"

// Temperature conversions
double celsius_to_fahrenheit(double celsius) {
    return (celsius * 9.0 / 5.0) + 32.0;
}

double fahrenheit_to_celsius(double fahrenheit) {
    return (fahrenheit - 32.0) * 5.0 / 9.0;
}

double celsius_to_kelvin(double celsius) {
    return celsius + 273.15;
}

double kelvin_to_celsius(double kelvin) {
    return kelvin - 273.15;
}

// Distance conversions
double km_to_miles(double km) {
    return km * 0.621371;
}

double miles_to_km(double miles) {
    return miles * 1.60934;
}

double meters_to_feet(double meters) {
    return meters * 3.28084;
}

double feet_to_meters(double feet) {
    return feet * 0.3048;
}

// Weight conversions
double kg_to_lbs(double kg) {
    return kg * 2.20462;
}

double lbs_to_kg(double lbs) {
    return lbs * 0.453592;
}

double kg_to_grams(double kg) {
    return kg * 1000.0;
}

double grams_to_kg(double grams) {
    return grams * 0.001;
}

// Volume conversions
double liters_to_gallons(double liters) {
    return liters * 0.264172;
}

double gallons_to_liters(double gallons) {
    return gallons * 3.78541;
}

double liters_to_ml(double liters) {
    return liters * 1000.0;
}

double ml_to_liters(double ml) {
    return ml * 0.001;
}

// Digital storage conversions
double mb_to_gb(double mb) {
    return mb * 0.001;
}

double gb_to_mb(double gb) {
    return gb * 1000.0;
}

double gb_to_tb(double gb) {
    return gb * 0.001;
}

double tb_to_gb(double tb) {
    return tb * 1000.0;
}

// Get conversion category
int get_conversion_category(void) {
    vga_println("=== Unit Converter ===");
    vga_println("Select category:");
    vga_println("1. Temperature");
    vga_println("2. Distance");
    vga_println("3. Weight");
    vga_println("4. Volume");
    vga_println("5. Digital Storage");
    vga_println("6. Exit");
    vga_print("Enter choice (1-6): ");
    
    while(1) {
        poll_keyboard();
        if (has_key()) {
            char c = getchar();
            vga_putchar(c);
            vga_println("");
            
            if (c >= '1' && c <= '6') {
                return c - '0';
            } else {
                vga_print("Invalid choice! Enter 1-6: ");
            }
        }
    }
}

// Get conversion type for temperature
int get_temperature_conversion(void) {
    vga_println("");
    vga_println("Temperature Conversions:");
    vga_println("1. Celsius to Fahrenheit");
    vga_println("2. Fahrenheit to Celsius");
    vga_println("3. Celsius to Kelvin");
    vga_println("4. Kelvin to Celsius");
    vga_println("5. Back to categories");
    vga_print("Enter choice (1-5): ");
    
    while(1) {
        poll_keyboard();
        if (has_key()) {
            char c = getchar();
            vga_putchar(c);
            vga_println("");
            
            if (c >= '1' && c <= '5') {
                return c - '0';
            } else {
                vga_print("Invalid choice! Enter 1-5: ");
            }
        }
    }
}

// Get conversion type for distance
int get_distance_conversion(void) {
    vga_println("");
    vga_println("Distance Conversions:");
    vga_println("1. Kilometers to Miles");
    vga_println("2. Miles to Kilometers");
    vga_println("3. Meters to Feet");
    vga_println("4. Feet to Meters");
    vga_println("5. Back to categories");
    vga_print("Enter choice (1-5): ");
    
    while(1) {
        poll_keyboard();
        if (has_key()) {
            char c = getchar();
            vga_putchar(c);
            vga_println("");
            
            if (c >= '1' && c <= '5') {
                return c - '0';
            } else {
                vga_print("Invalid choice! Enter 1-5: ");
            }
        }
    }
}

// Get conversion type for weight
int get_weight_conversion(void) {
    vga_println("");
    vga_println("Weight Conversions:");
    vga_println("1. Kilograms to Pounds");
    vga_println("2. Pounds to Kilograms");
    vga_println("3. Kilograms to Grams");
    vga_println("4. Grams to Kilograms");
    vga_println("5. Back to categories");
    vga_print("Enter choice (1-5): ");
    
    while(1) {
        poll_keyboard();
        if (has_key()) {
            char c = getchar();
            vga_putchar(c);
            vga_println("");
            
            if (c >= '1' && c <= '5') {
                return c - '0';
            } else {
                vga_print("Invalid choice! Enter 1-5: ");
            }
        }
    }
}

// Get conversion type for volume
int get_volume_conversion(void) {
    vga_println("");
    vga_println("Volume Conversions:");
    vga_println("1. Liters to Gallons");
    vga_println("2. Gallons to Liters");
    vga_println("3. Liters to Milliliters");
    vga_println("4. Milliliters to Liters");
    vga_println("5. Back to categories");
    vga_print("Enter choice (1-5): ");
    
    while(1) {
        poll_keyboard();
        if (has_key()) {
            char c = getchar();
            vga_putchar(c);
            vga_println("");
            
            if (c >= '1' && c <= '5') {
                return c - '0';
            } else {
                vga_print("Invalid choice! Enter 1-5: ");
            }
        }
    }
}

// Get conversion type for digital storage
int get_digital_conversion(void) {
    vga_println("");
    vga_println("Digital Storage Conversions:");
    vga_println("1. Megabytes to Gigabytes");
    vga_println("2. Gigabytes to Megabytes");
    vga_println("3. Gigabytes to Terabytes");
    vga_println("4. Terabytes to Gigabytes");
    vga_println("5. Back to categories");
    vga_print("Enter choice (1-5): ");
    
    while(1) {
        poll_keyboard();
        if (has_key()) {
            char c = getchar();
            vga_putchar(c);
            vga_println("");
            
            if (c >= '1' && c <= '5') {
                return c - '0';
            } else {
                vga_print("Invalid choice! Enter 1-5: ");
            }
        }
    }
}

// Get numeric input
double get_numeric_input(const char* prompt) {
    char buffer[32];
    int pos = 0;
    int decimal_points = 0;
    int has_digits = 0;
    
    vga_print(prompt);
    
    while(1) {
        poll_keyboard();
        
        if (has_key()) {
            char c = getchar();
            
            if (c == '\n') {
                if (pos == 0 || !has_digits) {
                    vga_println("Error: Please enter a valid number");
                    vga_print(prompt);
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

// Perform temperature conversion
void perform_temperature_conversion(int conversion_type) {
    if (conversion_type == 5) return;
    
    double input = get_numeric_input("Enter temperature: ");
    double result;
    const char* from_unit = "";
    const char* to_unit = "";
    
    switch(conversion_type) {
        case 1:
            result = celsius_to_fahrenheit(input);
            from_unit = "C";
            to_unit = "F";
            break;
        case 2:
            result = fahrenheit_to_celsius(input);
            from_unit = "F";
            to_unit = "C";
            break;
        case 3:
            result = celsius_to_kelvin(input);
            from_unit = "C";
            to_unit = "K";
            break;
        case 4:
            result = kelvin_to_celsius(input);
            from_unit = "K";
            to_unit = "C";
            break;
    }
    
    vga_print("Result: ");
    print_double(input);
    vga_print(" ");
    vga_print(from_unit);
    vga_print(" = ");
    print_double(result);
    vga_print(" ");
    vga_println(to_unit);
}

// Perform distance conversion
void perform_distance_conversion(int conversion_type) {
    if (conversion_type == 5) return;
    
    double input = get_numeric_input("Enter distance: ");
    double result;
    const char* from_unit = "";
    const char* to_unit = "";
    
    switch(conversion_type) {
        case 1:
            result = km_to_miles(input);
            from_unit = "km";
            to_unit = "miles";
            break;
        case 2:
            result = miles_to_km(input);
            from_unit = "miles";
            to_unit = "km";
            break;
        case 3:
            result = meters_to_feet(input);
            from_unit = "m";
            to_unit = "ft";
            break;
        case 4:
            result = feet_to_meters(input);
            from_unit = "ft";
            to_unit = "m";
            break;
    }
    
    vga_print("Result: ");
    print_double(input);
    vga_print(" ");
    vga_print(from_unit);
    vga_print(" = ");
    print_double(result);
    vga_print(" ");
    vga_println(to_unit);
}

// Perform weight conversion
void perform_weight_conversion(int conversion_type) {
    if (conversion_type == 5) return;
    
    double input = get_numeric_input("Enter weight: ");
    double result;
    const char* from_unit = "";
    const char* to_unit = "";
    
    switch(conversion_type) {
        case 1:
            result = kg_to_lbs(input);
            from_unit = "kg";
            to_unit = "lbs";
            break;
        case 2:
            result = lbs_to_kg(input);
            from_unit = "lbs";
            to_unit = "kg";
            break;
        case 3:
            result = kg_to_grams(input);
            from_unit = "kg";
            to_unit = "g";
            break;
        case 4:
            result = grams_to_kg(input);
            from_unit = "g";
            to_unit = "kg";
            break;
    }
    
    vga_print("Result: ");
    print_double(input);
    vga_print(" ");
    vga_print(from_unit);
    vga_print(" = ");
    print_double(result);
    vga_print(" ");
    vga_println(to_unit);
}

// Perform volume conversion
void perform_volume_conversion(int conversion_type) {
    if (conversion_type == 5) return;
    
    double input = get_numeric_input("Enter volume: ");
    double result;
    const char* from_unit = "";
    const char* to_unit = "";
    
    switch(conversion_type) {
        case 1:
            result = liters_to_gallons(input);
            from_unit = "L";
            to_unit = "gal";
            break;
        case 2:
            result = gallons_to_liters(input);
            from_unit = "gal";
            to_unit = "L";
            break;
        case 3:
            result = liters_to_ml(input);
            from_unit = "L";
            to_unit = "mL";
            break;
        case 4:
            result = ml_to_liters(input);
            from_unit = "mL";
            to_unit = "L";
            break;
    }
    
    vga_print("Result: ");
    print_double(input);
    vga_print(" ");
    vga_print(from_unit);
    vga_print(" = ");
    print_double(result);
    vga_print(" ");
    vga_println(to_unit);
}

// Perform digital storage conversion
void perform_digital_conversion(int conversion_type) {
    if (conversion_type == 5) return;
    
    double input = get_numeric_input("Enter size: ");
    double result;
    const char* from_unit = "";
    const char* to_unit = "";
    
    switch(conversion_type) {
        case 1:
            result = mb_to_gb(input);
            from_unit = "MB";
            to_unit = "GB";
            break;
        case 2:
            result = gb_to_mb(input);
            from_unit = "GB";
            to_unit = "MB";
            break;
        case 3:
            result = gb_to_tb(input);
            from_unit = "GB";
            to_unit = "TB";
            break;
        case 4:
            result = tb_to_gb(input);
            from_unit = "TB";
            to_unit = "GB";
            break;
    }
    
    vga_print("Result: ");
    print_double(input);
    vga_print(" ");
    vga_print(from_unit);
    vga_print(" = ");
    print_double(result);
    vga_print(" ");
    vga_println(to_unit);
}

// Main unit converter command
void unit_converter_command(void) {
    while(1) {
        int category = get_conversion_category();
        
        if (category == 6) {
            vga_println("=== Unit Converter Closed ===");
            break;
        }
        
        int conversion_type = 0;
        
        switch(category) {
            case 1: // Temperature
                conversion_type = get_temperature_conversion();
                if (conversion_type == 5) continue;
                perform_temperature_conversion(conversion_type);
                break;
                
            case 2: // Distance
                conversion_type = get_distance_conversion();
                if (conversion_type == 5) continue;
                perform_distance_conversion(conversion_type);
                break;
                
            case 3: // Weight
                conversion_type = get_weight_conversion();
                if (conversion_type == 5) continue;
                perform_weight_conversion(conversion_type);
                break;
                
            case 4: // Volume
                conversion_type = get_volume_conversion();
                if (conversion_type == 5) continue;
                perform_volume_conversion(conversion_type);
                break;
                
            case 5: // Digital Storage
                conversion_type = get_digital_conversion();
                if (conversion_type == 5) continue;
                perform_digital_conversion(conversion_type);
                break;
        }
        
        vga_println("");
        vga_println("Press any key to continue...");
        while(1) {
            poll_keyboard();
            if (has_key()) {
                getchar();
                break;
            }
        }
        vga_println("");
    }
}
