#include "keyboard.h"

#define KEYBOARD_DATA_PORT   0x60
#define KEYBOARD_STATUS_PORT 0x64
#define BUFFER_SIZE 256

static volatile char key_buffer[BUFFER_SIZE];
static volatile uint8_t head = 0;
static volatile uint8_t tail = 0;
static volatile int has_key_press = 0;
static volatile int shift_pressed = 0;
static volatile int caps_lock = 0;

// Extended key codes (0xE0 prefix)
#define EXTENDED_PREFIX      0xE0
#define KEY_UP               0x48
#define KEY_DOWN             0x50
#define KEY_LEFT             0x4B
#define KEY_RIGHT            0x4D
#define KEY_HOME             0x47
#define KEY_END              0x4F
#define KEY_DELETE           0x53
#define KEY_PAGEUP           0x49
#define KEY_PAGEDOWN         0x51
#define KEY_INSERT           0x52

// Regular key codes
#define KEY_LSHIFT          0x2A
#define KEY_RSHIFT          0x36  
#define KEY_CAPSLOCK        0x3A
#define KEY_LSHIFT_RELEASE  0xAA
#define KEY_RSHIFT_RELEASE  0xB6

// Function keys
#define KEY_F1  0x3B
#define KEY_F2  0x3C
#define KEY_F3  0x3D
#define KEY_F4  0x3E
#define KEY_F5  0x3F
#define KEY_F6  0x40
#define KEY_F7  0x41
#define KEY_F8  0x42
#define KEY_F9  0x43
#define KEY_F10 0x44
#define KEY_F11 0x57
#define KEY_F12 0x58

// Special characters for function keys and arrows
#define CHAR_F1     0x01
#define CHAR_F2     0x02  
#define CHAR_F3     0x03
#define CHAR_F4     0x04
#define CHAR_F5     0x05
#define CHAR_F6     0x06
#define CHAR_F7     0x07
#define CHAR_F8     0x08
#define CHAR_F9     0x09
#define CHAR_F10    0x0A
#define CHAR_F11    0x0B
#define CHAR_F12    0x0C
#define CHAR_UP     0x11
#define CHAR_DOWN   0x12
#define CHAR_LEFT   0x13
#define CHAR_RIGHT  0x14
#define CHAR_HOME   0x15
#define CHAR_END    0x16
#define CHAR_DELETE 0x17
#define CHAR_INSERT 0x18

// US keyboard map - lowercase
static const char keyboard_map_lower[128] = {
    0, 27, '1','2','3','4','5','6','7','8','9','0','-','=','\b','\t',
    'q','w','e','r','t','y','u','i','o','p','[',']','\n',0,
    'a','s','d','f','g','h','j','k','l',';','\'','`',0,'\\',
    'z','x','c','v','b','n','m',',','.','/',0,'*',0,' ',0,
    0,0,0,0,0,0,0,0,0,0,0,0,'7','8','9','-','4','5','6','+',
    '1','2','3','0','.',0,0,0,0,0
};

// US keyboard map - uppercase (with shift)
static const char keyboard_map_upper[128] = {
    0, 27, '!','@','#','$','%','^','&','*','(',')','_','+','\b','\t',
    'Q','W','E','R','T','Y','U','I','O','P','{','}','\n',0,
    'A','S','D','F','G','H','J','K','L',':','"','~',0,'|',
    'Z','X','C','V','B','N','M','<','>','?',0,'*',0,' ',0,
    0,0,0,0,0,0,0,0,0,0,0,0,'7','8','9','-','4','5','6','+',
    '1','2','3','0','.',0,0,0,0,0
};

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void keyboard_init(void) {
    // nothing to do for polling
}

void poll_keyboard(void) {
    static int extended_key = 0;
    
    uint8_t status = inb(KEYBOARD_STATUS_PORT);
    if (status & 0x01) {
        uint8_t code = inb(KEYBOARD_DATA_PORT);
        
        // Handle extended key prefix
        if (code == EXTENDED_PREFIX) {
            extended_key = 1;
            return;
        }
        
        // Handle modifier keys
        if (code == KEY_LSHIFT || code == KEY_RSHIFT) {
            shift_pressed = 1;
        }
        else if (code == KEY_LSHIFT_RELEASE || code == KEY_RSHIFT_RELEASE) {
            shift_pressed = 0;
        }
        else if (code == KEY_CAPSLOCK) {
            caps_lock = !caps_lock;
        }
        // Handle extended keys (arrows, etc.)
        else if (extended_key) {
            extended_key = 0;
            char special_char = 0;
            
            switch(code) {
                case KEY_UP:      special_char = CHAR_UP; break;
                case KEY_DOWN:    special_char = CHAR_DOWN; break;
                case KEY_LEFT:    special_char = CHAR_LEFT; break;
                case KEY_RIGHT:   special_char = CHAR_RIGHT; break;
                case KEY_HOME:    special_char = CHAR_HOME; break;
                case KEY_END:     special_char = CHAR_END; break;
                case KEY_DELETE:  special_char = CHAR_DELETE; break;
                case KEY_INSERT:  special_char = CHAR_INSERT; break;
            }
            
            if (special_char != 0) {
                key_buffer[head] = special_char;
                head = (head + 1) % BUFFER_SIZE;
                has_key_press = 1;
            }
        }
        // Handle function keys
        else if (code >= KEY_F1 && code <= KEY_F12) {
            char func_char = CHAR_F1 + (code - KEY_F1);
            key_buffer[head] = func_char;
            head = (head + 1) % BUFFER_SIZE;
            has_key_press = 1;
        }
        // Handle regular keys
        else if (code < 128) {
            char character = 0;
            
            // Determine character based on shift and caps lock
            int use_upper = shift_pressed;
            if (caps_lock) {
                // Caps lock affects only letters
                if ((code >= 0x10 && code <= 0x1C) ||  // qwertyuiop
                    (code >= 0x1E && code <= 0x26) ||  // asdfghjkl
                    (code >= 0x2C && code <= 0x32)) {  // zxcvbnm
                    use_upper = !use_upper;
                }
            }
            
            if (use_upper) {
                character = keyboard_map_upper[code];
            } else {
                character = keyboard_map_lower[code];
            }
            
            if (character != 0) {
                key_buffer[head] = character;
                head = (head + 1) % BUFFER_SIZE;
                has_key_press = 1;
            }
        }
    }
}

char getchar(void) {
    if (head == tail) return 0;
    char c = key_buffer[tail];
    tail = (tail + 1) % BUFFER_SIZE;
    if (head == tail) has_key_press = 0;
    return c;
}

int has_key(void) {
    return has_key_press;
}

int is_shift_pressed(void) {
    return shift_pressed;
}

int is_caps_lock_on(void) {
    return caps_lock;
}
