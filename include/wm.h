#ifndef WM_H
#define WM_H

#include <stdint.h>
#include <stddef.h>

#define MAX_WINDOWS 16

#define WM_TITLEBAR_HEIGHT 32
#define WM_BORDER_WIDTH     3
#define WM_RESIZE_BORDER    6

// Mouse event types for windows
#define WM_EVENT_MOUSE_MOVE  1
#define WM_EVENT_MOUSE_DOWN  2
#define WM_EVENT_MOUSE_UP    3
#define WM_EVENT_MOUSE_CLICK 4
#define WM_EVENT_SCROLL_UP   5
#define WM_EVENT_SCROLL_DOWN 6
#define WM_EVENT_MOUSE_DRAG  7

typedef enum {
    WM_RESIZE_NONE = 0,
    WM_RESIZE_N    = 1,
    WM_RESIZE_S    = 2,
    WM_RESIZE_W    = 3,
    WM_RESIZE_E    = 4,
    WM_RESIZE_NW   = 5,
    WM_RESIZE_NE   = 6,
    WM_RESIZE_SW   = 7,
    WM_RESIZE_SE   = 8
} wm_resize_mode_t;

typedef enum {
    WM_SNAP_NONE         = 0,
    WM_SNAP_LEFT         = 1,
    WM_SNAP_RIGHT        = 2,
    WM_SNAP_MAX          = 3,
    WM_SNAP_TOP_LEFT     = 4,
    WM_SNAP_TOP_RIGHT    = 5,
    WM_SNAP_BOTTOM_LEFT  = 6,
    WM_SNAP_BOTTOM_RIGHT = 7
} wm_snap_state_t;

typedef enum {
    WM_ANIM_NONE       = 0,
    WM_ANIM_OPENING    = 1,
    WM_ANIM_CLOSING    = 2,
    WM_ANIM_MINIMIZING = 3,
    WM_ANIM_RESTORING  = 4,
    WM_ANIM_MAXIMIZING = 5
} wm_anim_type_t;

struct window;
typedef struct window window_t;

typedef void (*wm_render_fn)(window_t* win, int client_x, int client_y, int client_w, int client_h);
typedef void (*wm_mouse_fn)(window_t* win, int rel_x, int rel_y, int buttons, int event_type);
typedef void (*wm_key_fn)(window_t* win, char key);
typedef void (*wm_close_fn)(window_t* win);

struct window {
    int id;
    char title[64];
    int icon_id;

    // Window dimensions and position
    int x, y;
    int width, height;

    // Stored dimensions for restore from maximized / snapped
    int orig_x, orig_y;
    int orig_w, orig_h;

    // Minimum dimensions
    int min_w, min_h;

    // State flags
    int is_active;
    int is_minimized;
    int is_maximized;
    int is_closed;
    int is_dragging;
    int drag_off_x, drag_off_y;

    // Resizing state
    int is_resizing;
    wm_resize_mode_t resize_mode;
    int resize_start_mx, resize_start_my;
    int resize_orig_x, resize_orig_y;
    int resize_orig_w, resize_orig_h;

    // Snap state (0=none, 1=left, 2=right, 3=max, 4..7=quadrants)
    wm_snap_state_t snap_state;

    // Compositor animation & physics state
    wm_anim_type_t anim_type;
    int anim_progress;   // 0 to 100
    int anim_target_x, anim_target_y, anim_target_w, anim_target_h;
    int anim_start_x, anim_start_y, anim_start_w, anim_start_h;
    uint8_t opacity;     // Window master opacity (0 to 255, 220 when dragging)

    // Callbacks
    wm_render_fn render;
    wm_mouse_fn  on_mouse;
    wm_key_fn    on_key;
    wm_close_fn  on_close;

    // Application context
    void* userdata;
};

// Window Manager API
void      wm_init(void);
window_t* wm_create_window(const char* title, int icon_id, int x, int y, int w, int h, wm_render_fn render);
void      wm_destroy_window(window_t* win);

void      wm_bring_to_front(window_t* win);
window_t* wm_get_active_window(void);
void      wm_set_active_window(window_t* win);

void      wm_minimize_window(window_t* win);
void      wm_maximize_window(window_t* win);
void      wm_restore_window(window_t* win);
void      wm_close_window(window_t* win);
void      wm_tile_left(window_t* win);
void      wm_tile_right(window_t* win);
void      wm_minimize_all(void);
void      wm_restore_all(void);

void      wm_handle_mouse(int mouse_x, int mouse_y, int buttons, int clicked, int released, int scroll_dz);
void      wm_handle_key(char key);
int       wm_has_window_at(int mouse_x, int mouse_y);

void      wm_draw_snap_preview(void);
void      wm_render_windows(void);
void      wm_update_animations(void);

// Alt-Tab Switcher HUD
void      wm_switcher_open(void);
void      wm_switcher_next(void);
void      wm_switcher_close(void);
int       wm_switcher_is_open(void);
void      wm_draw_switcher(void);
void      wm_switcher_select_index(int idx);

// Mission Control / Window Overview Mode
void      wm_overview_open(void);
void      wm_overview_close(void);
void      wm_overview_toggle(void);
int       wm_overview_is_open(void);
void      wm_draw_overview(int mouse_x, int mouse_y);
int       wm_overview_handle_mouse(int mouse_x, int mouse_y, int clicked);

int       wm_get_window_count(void);
window_t* wm_get_window(int index);

#endif
