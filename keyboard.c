#include "keyboard.h"
#include "mouse.h"

#define KEYBOARD_DATA_PORT   0x60
#define KEYBOARD_STATUS_PORT 0x64
#define BUFFER_SIZE 256

static volatile char key_buffer[BUFFER_SIZE];
static volatile uint8_t head = 0;
static volatile uint8_t tail = 0;
static volatile int has_key_press = 0;
static volatile int shift_pressed = 0;
static volatile int caps_lock = 0;
static volatile int ctrl_pressed = 0;
static volatile int alt_pressed = 0;
static volatile int alt_f4_flag = 0;

int check_and_clear_alt_f4(void) {
    if (alt_f4_flag) {
        alt_f4_flag = 0;
        return 1;
    }
    return 0;
}

typedef struct {
    uint8_t pressed;
    uint8_t doom_key;
} key_event_t;

#define EVENT_QUEUE_SIZE 128
static volatile key_event_t event_queue[EVENT_QUEUE_SIZE];
static volatile uint8_t event_head = 0;
static volatile uint8_t event_tail = 0;

static void push_key_event(int pressed, uint8_t dkey) {
    if (!dkey) return;
    uint8_t next = (event_head + 1) % EVENT_QUEUE_SIZE;
    if (next != event_tail) {
        event_queue[event_head].pressed = pressed ? 1 : 0;
        event_queue[event_head].doom_key = dkey;
        event_head = next;
    }
}

static uint8_t scancode_to_doom_key(uint8_t sc, int is_extended) {
    (void)is_extended;
    switch (sc) {
        case 0x48: return 0xad; // KEY_UPARROW
        case 0x50: return 0xaf; // KEY_DOWNARROW
        case 0x4B: return 0xac; // KEY_LEFTARROW
        case 0x4D: return 0xae; // KEY_RIGHTARROW
        case 0x01: return 27;   // KEY_ESCAPE
        case 0x1C: return 13;   // KEY_ENTER
        case 0x39: return 0xa2; // KEY_USE (Space)
        case 0x1D: return 0xa3; // KEY_FIRE (Ctrl)
        case 0x2A: case 0x36: return 0x80+0x36; // KEY_RSHIFT
        case 0x38: return 0x80+0x38; // KEY_RALT
        case 0x0F: return 9;    // KEY_TAB
        case 0x0E: return 0x7f; // Backspace
        case 0x33: return ',';  // KEY_STRAFE_L
        case 0x34: return '.';  // KEY_STRAFE_R
        // Numbers 1-0
        case 0x02: return '1'; case 0x03: return '2'; case 0x04: return '3'; case 0x05: return '4';
        case 0x06: return '5'; case 0x07: return '6'; case 0x08: return '7'; case 0x09: return '8';
        case 0x0A: return '9'; case 0x0B: return '0';
        // Letters QWERTYUIOP
        case 0x10: return 'q'; case 0x11: return 'w'; case 0x12: return 'e'; case 0x13: return 'r';
        case 0x14: return 't'; case 0x15: return 'y'; case 0x16: return 'u'; case 0x17: return 'i';
        case 0x18: return 'o'; case 0x19: return 'p';
        // ASDFGHJKL
        case 0x1E: return 'a'; case 0x1F: return 's'; case 0x20: return 'd'; case 0x21: return 'f';
        case 0x22: return 'g'; case 0x23: return 'h'; case 0x24: return 'j'; case 0x25: return 'k';
        case 0x26: return 'l';
        // ZXCVBNM
        case 0x2C: return 'z'; case 0x2D: return 'x'; case 0x2E: return 'c'; case 0x2F: return 'v';
        case 0x30: return 'b'; case 0x31: return 'n'; case 0x32: return 'm';
        // F1-F12
        case 0x3B: return 0x80+0x3b; case 0x3C: return 0x80+0x3c; case 0x3D: return 0x80+0x3d;
        case 0x3E: return 0x80+0x3e; case 0x3F: return 0x80+0x3f; case 0x40: return 0x80+0x40;
        case 0x41: return 0x80+0x41; case 0x42: return 0x80+0x42; case 0x43: return 0x80+0x43;
        case 0x44: return 0x80+0x44; case 0x57: return 0x80+0x57; case 0x58: return 0x80+0x58;
        default: return 0;
    }
}

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
#define CHAR_HOME     0x15
#define CHAR_END      0x16
#define CHAR_DELETE   0x17
#define CHAR_INSERT   0x18
#define CHAR_PAGEUP   0x19
#define CHAR_PAGEDOWN 0x1A

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
    
    // Poll COM1 (0x3F8) for serial console input
    while (inb(0x3FD) & 0x01) {
        char sc = (char)inb(0x3F8);
        if (sc == '\r') sc = '\n';
        key_buffer[head] = sc;
        head = (head + 1) % BUFFER_SIZE;
        has_key_press = 1;

        // Also push to Doom key event queue
        uint8_t dkey = 0;
        if (sc == '\n' || sc == '\r') dkey = 13;
        else if (sc == 27) dkey = 27;
        else if (sc == ' ') dkey = 0xa2;
        else if (sc >= 'A' && sc <= 'Z') dkey = sc + 32;
        else if (sc >= 'a' && sc <= 'z') dkey = sc;
        else if (sc >= '0' && sc <= '9') dkey = sc;
        else if (sc == ',' || sc == '.') dkey = sc;
        if (dkey) {
            push_key_event(1, dkey);
            push_key_event(0, dkey);
        }
    }

    int max_scans = 16;
    while (--max_scans) {
        uint8_t status = inb(KEYBOARD_STATUS_PORT);
        if (!(status & 0x01)) {
            break;
        }

        // If mouse data, check if mouse driver is active
        if (status & 0x20) {
            if (mouse_is_enabled()) {
                // Leave data in port 0x60 for mouse_poll()!
                break;
            }
            // If mouse is not enabled, discard it so buffer does not clog
            inb(KEYBOARD_DATA_PORT);
            continue;
        }

        uint8_t code = inb(KEYBOARD_DATA_PORT);

        // Handle extended key prefix
        if (code == EXTENDED_PREFIX) {
            extended_key = 1;
            continue;
        }

        // Push to Doom key event queue
        int is_break = (code & 0x80) != 0;
        uint8_t sc = (code & 0x7F);

        // Intercept Alt+F4 at hardware scancode level
        if (sc == KEY_F4 && alt_pressed && !is_break) {
            extern void doom_exit(void);
            doom_exit();
            alt_f4_flag = 1;
            key_buffer[head] = CHAR_F4;
            head = (head + 1) % BUFFER_SIZE;
            has_key_press = 1;
            continue;
        }

        uint8_t dkey = scancode_to_doom_key(sc, extended_key);
        if (dkey) {
            push_key_event(!is_break, dkey);
        }
        
        // Handle modifier keys
        if (code == KEY_LSHIFT || code == KEY_RSHIFT) {
            shift_pressed = 1;
        }
        else if (code == KEY_LSHIFT_RELEASE || code == KEY_RSHIFT_RELEASE) {
            shift_pressed = 0;
        }
        else if (code == 0x1D) { // Ctrl pressed
            ctrl_pressed = 1;
        }
        else if (code == 0x9D) { // Ctrl released
            ctrl_pressed = 0;
        }
        else if (code == 0x38) { // Alt pressed
            alt_pressed = 1;
        }
        else if (code == 0xB8) { // Alt released
            alt_pressed = 0;
        }
        else if (code == KEY_CAPSLOCK) {
            caps_lock = !caps_lock;
        }
        // Handle extended keys (arrows, etc.)
        else if (extended_key) {
            extended_key = 0;
            char special_char = 0;
            switch(code) {
                case KEY_UP:        special_char = CHAR_UP; break;
                case KEY_DOWN:      special_char = CHAR_DOWN; break;
                case KEY_LEFT:      special_char = CHAR_LEFT; break;
                case KEY_RIGHT:     special_char = CHAR_RIGHT; break;
                case KEY_HOME:      special_char = CHAR_HOME; break;
                case KEY_END:       special_char = CHAR_END; break;
                case KEY_PAGEUP:    special_char = CHAR_PAGEUP; break;
                case KEY_PAGEDOWN:  special_char = CHAR_PAGEDOWN; break;
                case KEY_DELETE:    special_char = CHAR_DELETE; break;
                case KEY_INSERT:    special_char = CHAR_INSERT; break;
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

int is_ctrl_pressed(void) {
    return ctrl_pressed;
}

int is_alt_pressed(void) {
    return alt_pressed;
}

int keyboard_poll_event(int* pressed, unsigned char* doom_key) {
    mouse_poll();
    poll_keyboard();
    if (event_head == event_tail) return 0;
    if (pressed) *pressed = event_queue[event_tail].pressed;
    if (doom_key) *doom_key = event_queue[event_tail].doom_key;
    event_tail = (event_tail + 1) % EVENT_QUEUE_SIZE;
    return 1;
}

void keyboard_clear_events(void) {
    event_head = 0;
    event_tail = 0;
}
