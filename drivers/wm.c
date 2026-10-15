#include "wm.h"
#include "graphics.h"
#include "font.h"
#include "icons.h"
#include "string.h"
#include "serial.h"
#include "keyboard.h"
#include <stdint.h>
#include <stddef.h>

static window_t window_pool[MAX_WINDOWS];
static window_t* window_stack[MAX_WINDOWS];
static int window_count = 0;
static window_t* active_window = NULL;
static window_t* dragging_window = NULL;
static window_t* resizing_window = NULL;

static wm_snap_state_t pending_snap = WM_SNAP_NONE;

// Switcher HUD state
static int switcher_open = 0;
static int switcher_selected_idx = 0;

void wm_init(void) {
    window_count = 0;
    active_window = NULL;
    dragging_window = NULL;
    resizing_window = NULL;
    pending_snap = WM_SNAP_NONE;
    switcher_open = 0;
    switcher_selected_idx = 0;

    for (int i = 0; i < MAX_WINDOWS; i++) {
        window_pool[i].is_closed = 1;
        window_pool[i].id = i;
        window_stack[i] = NULL;
    }
}

window_t* wm_create_window(const char* title, int icon_id, int x, int y, int w, int h, wm_render_fn render) {
    if (window_count >= MAX_WINDOWS) return NULL;

    // Find free window in pool
    window_t* win = NULL;
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (window_pool[i].is_closed) {
            win = &window_pool[i];
            break;
        }
    }
    if (!win) return NULL;

    // Initialize title
    int len = 0;
    while (title[len] && len < 63) {
        win->title[len] = title[len];
        len++;
    }
    win->title[len] = '\0';

    win->icon_id = icon_id;
    win->min_w = 220;
    win->min_h = 130;

    win->x = x;
    win->y = (y < 30) ? 30 : y;
    win->width = (w < win->min_w) ? win->min_w : w;
    win->height = (h < win->min_h) ? win->min_h : h;

    win->orig_x = win->x;
    win->orig_y = win->y;
    win->orig_w = win->width;
    win->orig_h = win->height;

    win->is_active = 1;
    win->is_minimized = 0;
    win->is_maximized = 0;
    win->is_closed = 0;
    win->is_dragging = 0;
    win->drag_off_x = 0;
    win->drag_off_y = 0;

    win->is_resizing = 0;
    win->resize_mode = WM_RESIZE_NONE;
    win->snap_state = WM_SNAP_NONE;

    win->anim_type = WM_ANIM_OPENING;
    win->anim_progress = 0;
    win->opacity = 255;

    win->render = render;
    win->on_mouse = NULL;
    win->on_key = NULL;
    win->on_close = NULL;
    win->userdata = NULL;

    // Add to stack on top
    window_stack[window_count++] = win;
    wm_bring_to_front(win);

    return win;
}

void wm_destroy_window(window_t* win) {
    if (!win || win->is_closed) return;

    if (win->on_close) {
        win->on_close(win);
    }

    win->is_closed = 1;
    if (dragging_window == win) dragging_window = NULL;
    if (resizing_window == win) resizing_window = NULL;

    // Remove from stack
    int idx = -1;
    for (int i = 0; i < window_count; i++) {
        if (window_stack[i] == win) {
            idx = i;
            break;
        }
    }

    if (idx != -1) {
        for (int i = idx; i < window_count - 1; i++) {
            window_stack[i] = window_stack[i + 1];
        }
        window_count--;
        window_stack[window_count] = NULL;
    }

    // Set new active window
    if (active_window == win) {
        active_window = NULL;
        for (int i = window_count - 1; i >= 0; i--) {
            if (!window_stack[i]->is_minimized && !window_stack[i]->is_closed) {
                wm_set_active_window(window_stack[i]);
                break;
            }
        }
    }
}

void wm_bring_to_front(window_t* win) {
    if (!win || win->is_closed) return;

    int idx = -1;
    for (int i = 0; i < window_count; i++) {
        if (window_stack[i] == win) {
            idx = i;
            break;
        }
    }
    if (idx == -1) return;

    // Shift to top
    for (int i = idx; i < window_count - 1; i++) {
        window_stack[i] = window_stack[i + 1];
    }
    window_stack[window_count - 1] = win;
    wm_set_active_window(win);
}

window_t* wm_get_active_window(void) {
    return active_window;
}

void wm_set_active_window(window_t* win) {
    if (active_window) {
        active_window->is_active = 0;
    }
    active_window = win;
    if (win) {
        win->is_active = 1;
        win->is_minimized = 0;
    }
}

void wm_minimize_window(window_t* win) {
    if (!win || win->is_closed) return;
    win->anim_type = WM_ANIM_MINIMIZING;
    win->anim_progress = 0;
    win->anim_start_x = win->x;
    win->anim_start_y = win->y;
    win->anim_start_w = win->width;
    win->anim_start_h = win->height;
    win->anim_target_x = win->x + win->width / 2;
    win->anim_target_y = SCREEN_HEIGHT - 30;
    win->anim_target_w = 40;
    win->anim_target_h = 20;
    win->is_active = 0;

    if (active_window == win) {
        active_window = NULL;
        for (int i = window_count - 1; i >= 0; i--) {
            if (!window_stack[i]->is_minimized && !window_stack[i]->is_closed && window_stack[i] != win) {
                wm_set_active_window(window_stack[i]);
                break;
            }
        }
    }
}

void wm_maximize_window(window_t* win) {
    if (!win || win->is_closed) return;

    if (win->is_maximized) {
        wm_restore_window(win);
        return;
    }

    if (win->snap_state == WM_SNAP_NONE) {
        win->orig_x = win->x;
        win->orig_y = win->y;
        win->orig_w = win->width;
        win->orig_h = win->height;
    }

    // Maximize to full desktop area (leaving top bar 30px and taskbar 52px)
    win->x = 0;
    win->y = 30;
    win->width = SCREEN_WIDTH;
    win->height = SCREEN_HEIGHT - 30 - 52;
    win->is_maximized = 1;
    win->snap_state = WM_SNAP_MAX;
    wm_bring_to_front(win);
}

void wm_restore_window(window_t* win) {
    if (!win || win->is_closed) return;
    win->is_minimized = 0;
    win->anim_type = WM_ANIM_RESTORING;
    win->anim_progress = 0;
    win->anim_start_x = win->orig_x + win->orig_w / 2;
    win->anim_start_y = SCREEN_HEIGHT - 30;
    win->anim_start_w = 40;
    win->anim_start_h = 20;
    win->anim_target_x = win->orig_x;
    win->anim_target_y = win->orig_y;
    win->anim_target_w = win->orig_w;
    win->anim_target_h = win->orig_h;
    win->x = win->orig_x;
    win->y = win->orig_y;
    win->width = win->orig_w;
    win->height = win->orig_h;
    win->is_maximized = 0;
    win->snap_state = WM_SNAP_NONE;
    wm_bring_to_front(win);
}

void wm_tile_left(window_t* win) {
    if (!win || win->is_closed) return;
    if (win->snap_state == WM_SNAP_NONE && !win->is_maximized) {
        win->orig_x = win->x;
        win->orig_y = win->y;
        win->orig_w = win->width;
        win->orig_h = win->height;
    }
    win->x = 0;
    win->y = 30;
    win->width = SCREEN_WIDTH / 2;
    win->height = SCREEN_HEIGHT - 30 - 52;
    win->is_maximized = 0;
    win->snap_state = WM_SNAP_LEFT;
    wm_bring_to_front(win);
}

void wm_tile_right(window_t* win) {
    if (!win || win->is_closed) return;
    if (win->snap_state == WM_SNAP_NONE && !win->is_maximized) {
        win->orig_x = win->x;
        win->orig_y = win->y;
        win->orig_w = win->width;
        win->orig_h = win->height;
    }
    win->x = SCREEN_WIDTH / 2;
    win->y = 30;
    win->width = SCREEN_WIDTH / 2;
    win->height = SCREEN_HEIGHT - 30 - 52;
    win->is_maximized = 0;
    win->snap_state = WM_SNAP_RIGHT;
    wm_bring_to_front(win);
}

void wm_tile_top_left(window_t* win) {
    if (!win || win->is_closed) return;
    if (win->snap_state == WM_SNAP_NONE && !win->is_maximized) {
        win->orig_x = win->x;
        win->orig_y = win->y;
        win->orig_w = win->width;
        win->orig_h = win->height;
    }
    int avail_h = SCREEN_HEIGHT - 30 - 52;
    win->x = 0;
    win->y = 30;
    win->width = SCREEN_WIDTH / 2;
    win->height = avail_h / 2;
    win->is_maximized = 0;
    win->snap_state = WM_SNAP_TOP_LEFT;
    wm_bring_to_front(win);
}

void wm_tile_top_right(window_t* win) {
    if (!win || win->is_closed) return;
    if (win->snap_state == WM_SNAP_NONE && !win->is_maximized) {
        win->orig_x = win->x;
        win->orig_y = win->y;
        win->orig_w = win->width;
        win->orig_h = win->height;
    }
    int avail_h = SCREEN_HEIGHT - 30 - 52;
    win->x = SCREEN_WIDTH / 2;
    win->y = 30;
    win->width = SCREEN_WIDTH / 2;
    win->height = avail_h / 2;
    win->is_maximized = 0;
    win->snap_state = WM_SNAP_TOP_RIGHT;
    wm_bring_to_front(win);
}

void wm_tile_bottom_left(window_t* win) {
    if (!win || win->is_closed) return;
    if (win->snap_state == WM_SNAP_NONE && !win->is_maximized) {
        win->orig_x = win->x;
        win->orig_y = win->y;
        win->orig_w = win->width;
        win->orig_h = win->height;
    }
    int avail_h = SCREEN_HEIGHT - 30 - 52;
    win->x = 0;
    win->y = 30 + avail_h / 2;
    win->width = SCREEN_WIDTH / 2;
    win->height = avail_h - avail_h / 2;
    win->is_maximized = 0;
    win->snap_state = WM_SNAP_BOTTOM_LEFT;
    wm_bring_to_front(win);
}

void wm_tile_bottom_right(window_t* win) {
    if (!win || win->is_closed) return;
    if (win->snap_state == WM_SNAP_NONE && !win->is_maximized) {
        win->orig_x = win->x;
        win->orig_y = win->y;
        win->orig_w = win->width;
        win->orig_h = win->height;
    }
    int avail_h = SCREEN_HEIGHT - 30 - 52;
    win->x = SCREEN_WIDTH / 2;
    win->y = 30 + avail_h / 2;
    win->width = SCREEN_WIDTH / 2;
    win->height = avail_h - avail_h / 2;
    win->is_maximized = 0;
    win->snap_state = WM_SNAP_BOTTOM_RIGHT;
    wm_bring_to_front(win);
}

void wm_update_animations(void) {
    for (int i = 0; i < window_count; i++) {
        window_t* win = window_stack[i];
        if (!win || win->is_closed) continue;

        if (win->anim_type != WM_ANIM_NONE) {
            win->anim_progress += 25; // 4-frame super-smooth transition
            if (win->anim_progress >= 100) {
                win->anim_progress = 100;
                if (win->anim_type == WM_ANIM_MINIMIZING) {
                    win->is_minimized = 1;
                }
                win->anim_type = WM_ANIM_NONE;
            }
        }
    }
}

void wm_minimize_all(void) {
    for (int i = 0; i < window_count; i++) {
        if (window_stack[i] && !window_stack[i]->is_closed) {
            window_stack[i]->is_minimized = 1;
            window_stack[i]->is_active = 0;
        }
    }
    active_window = NULL;
}

void wm_restore_all(void) {
    for (int i = 0; i < window_count; i++) {
        if (window_stack[i] && !window_stack[i]->is_closed) {
            window_stack[i]->is_minimized = 0;
        }
    }
    if (window_count > 0 && window_stack[window_count - 1]) {
        wm_set_active_window(window_stack[window_count - 1]);
    }
}

void wm_close_window(window_t* win) {
    wm_destroy_window(win);
}

// Border resize hit tester (8 directions)
static wm_resize_mode_t wm_hit_test_border(window_t* win, int mx, int my) {
    if (!win || win->is_maximized || win->snap_state != WM_SNAP_NONE) return WM_RESIZE_NONE;

    int bx = win->x;
    int by = win->y;
    int bw = win->width;
    int bh = win->height;
    int b = WM_RESIZE_BORDER;

    // Outside total bounding box + margin
    if (mx < bx - b || mx > bx + bw + b || my < by - b || my > by + bh + b) {
        return WM_RESIZE_NONE;
    }

    // Inside client area (not on border)
    if (mx >= bx + b && mx < bx + bw - b && my >= by + b && my < by + bh - b) {
        return WM_RESIZE_NONE;
    }

    int on_left   = (mx >= bx - b && mx < bx + b);
    int on_right  = (mx >= bx + bw - b && mx <= bx + bw + b);
    int on_top    = (my >= by - b && my < by + b);
    int on_bottom = (my >= by + bh - b && my <= by + bh + b);

    if (on_top && on_left)     return WM_RESIZE_NW;
    if (on_top && on_right)    return WM_RESIZE_NE;
    if (on_bottom && on_left)  return WM_RESIZE_SW;
    if (on_bottom && on_right) return WM_RESIZE_SE;

    if (on_top)    return WM_RESIZE_N;
    if (on_bottom) return WM_RESIZE_S;
    if (on_left)   return WM_RESIZE_W;
    if (on_right)  return WM_RESIZE_E;

    return WM_RESIZE_NONE;
}

int wm_has_window_at(int mouse_x, int mouse_y) {
    for (int i = window_count - 1; i >= 0; i--) {
        window_t* win = window_stack[i];
        if (!win || win->is_closed || win->is_minimized) continue;
        if (mouse_x >= win->x && mouse_x < win->x + win->width &&
            mouse_y >= win->y && mouse_y < win->y + win->height) {
            return 1;
        }
    }
    return 0;
}

void wm_handle_mouse(int mouse_x, int mouse_y, int buttons, int clicked, int released, int scroll_dz) {
    // 0. Dispatch Scroll Wheel Events to topmost window under mouse
    if (scroll_dz != 0) {
        for (int i = window_count - 1; i >= 0; i--) {
            window_t* win = window_stack[i];
            if (!win || win->is_closed || win->is_minimized) continue;
            if (mouse_x >= win->x && mouse_x < win->x + win->width &&
                mouse_y >= win->y && mouse_y < win->y + win->height) {
                if (win->on_mouse) {
                    int rx = mouse_x - (win->x + WM_BORDER_WIDTH);
                    int ry = mouse_y - (win->y + WM_TITLEBAR_HEIGHT);
                    int ev = (scroll_dz > 0) ? WM_EVENT_SCROLL_UP : WM_EVENT_SCROLL_DOWN;
                    win->on_mouse(win, rx, ry, buttons, ev);
                }
                break;
            }
        }
    }

    // 1. Handle active window resizing
    if (resizing_window) {
        if (buttons & 1) { // Left button still held
            int dx = mouse_x - resizing_window->resize_start_mx;
            int dy = mouse_y - resizing_window->resize_start_my;

            int new_x = resizing_window->resize_orig_x;
            int new_y = resizing_window->resize_orig_y;
            int new_w = resizing_window->resize_orig_w;
            int new_h = resizing_window->resize_orig_h;

            switch (resizing_window->resize_mode) {
                case WM_RESIZE_E:
                    new_w = resizing_window->resize_orig_w + dx;
                    break;
                case WM_RESIZE_S:
                    new_h = resizing_window->resize_orig_h + dy;
                    break;
                case WM_RESIZE_SE:
                    new_w = resizing_window->resize_orig_w + dx;
                    new_h = resizing_window->resize_orig_h + dy;
                    break;
                case WM_RESIZE_W:
                    new_x = resizing_window->resize_orig_x + dx;
                    new_w = resizing_window->resize_orig_w - dx;
                    break;
                case WM_RESIZE_N:
                    new_y = resizing_window->resize_orig_y + dy;
                    new_h = resizing_window->resize_orig_h - dy;
                    break;
                case WM_RESIZE_NW:
                    new_x = resizing_window->resize_orig_x + dx;
                    new_w = resizing_window->resize_orig_w - dx;
                    new_y = resizing_window->resize_orig_y + dy;
                    new_h = resizing_window->resize_orig_h - dy;
                    break;
                case WM_RESIZE_NE:
                    new_w = resizing_window->resize_orig_w + dx;
                    new_y = resizing_window->resize_orig_y + dy;
                    new_h = resizing_window->resize_orig_h - dy;
                    break;
                case WM_RESIZE_SW:
                    new_x = resizing_window->resize_orig_x + dx;
                    new_w = resizing_window->resize_orig_w - dx;
                    new_h = resizing_window->resize_orig_h + dy;
                    break;
                default:
                    break;
            }

            // Enforce minimum dimensions
            if (new_w < resizing_window->min_w) {
                if (resizing_window->resize_mode == WM_RESIZE_W || resizing_window->resize_mode == WM_RESIZE_NW || resizing_window->resize_mode == WM_RESIZE_SW) {
                    new_x = resizing_window->resize_orig_x + (resizing_window->resize_orig_w - resizing_window->min_w);
                }
                new_w = resizing_window->min_w;
            }
            if (new_h < resizing_window->min_h) {
                if (resizing_window->resize_mode == WM_RESIZE_N || resizing_window->resize_mode == WM_RESIZE_NW || resizing_window->resize_mode == WM_RESIZE_NE) {
                    new_y = resizing_window->resize_orig_y + (resizing_window->resize_orig_h - resizing_window->min_h);
                }
                new_h = resizing_window->min_h;
            }

            // Keep within desktop top/bottom
            if (new_y < 30) {
                new_h -= (30 - new_y);
                new_y = 30;
            }
            if (new_y + new_h > SCREEN_HEIGHT - 52) {
                new_h = SCREEN_HEIGHT - 52 - new_y;
            }

            resizing_window->x = new_x;
            resizing_window->y = new_y;
            resizing_window->width = new_w;
            resizing_window->height = new_h;
            return;
        } else {
            // Resize completed
            resizing_window->orig_w = resizing_window->width;
            resizing_window->orig_h = resizing_window->height;
            resizing_window->orig_x = resizing_window->x;
            resizing_window->orig_y = resizing_window->y;
            resizing_window->is_resizing = 0;
            resizing_window->resize_mode = WM_RESIZE_NONE;
            resizing_window = NULL;
        }
    }

    // 2. Handle active window dragging, Aero Shake & Aero Snap detection
    if (dragging_window) {
        if (buttons & 1) { // Left button still held
            int new_x = mouse_x - dragging_window->drag_off_x;
            int new_y = mouse_y - dragging_window->drag_off_y;

            // Keep within bounds
            if (new_y < 30) new_y = 30;
            if (new_y > SCREEN_HEIGHT - 60) new_y = SCREEN_HEIGHT - 60;
            if (new_x < -dragging_window->width + 60) new_x = -dragging_window->width + 60;
            if (new_x > SCREEN_WIDTH - 60) new_x = SCREEN_WIDTH - 60;

            dragging_window->x = new_x;
            dragging_window->y = new_y;
            dragging_window->opacity = 220; // Sleek translucent glass while dragging

            // Aero Shake detection (shaking window minimizes all others)
            static int shake_prev_x = 0;
            static int shake_prev_dir = 0;
            static int shake_reversals = 0;
            int cur_dir = (mouse_x > shake_prev_x + 6) ? 1 : ((mouse_x < shake_prev_x - 6) ? -1 : 0);
            if (cur_dir != 0 && cur_dir != shake_prev_dir) {
                shake_reversals++;
                if (shake_reversals >= 5) {
                    for (int w = 0; w < window_count; w++) {
                        if (window_stack[w] != dragging_window && !window_stack[w]->is_closed && !window_stack[w]->is_minimized) {
                            wm_minimize_window(window_stack[w]);
                        }
                    }
                    shake_reversals = 0;
                }
                shake_prev_dir = cur_dir;
                shake_prev_x = mouse_x;
            }

            // Aero Snap 4-Corner Quadrant & Edge Detection
            int corner_margin = 28;
            if (mouse_x <= corner_margin && mouse_y <= 75) {
                pending_snap = WM_SNAP_TOP_LEFT;
            } else if (mouse_x >= SCREEN_WIDTH - corner_margin && mouse_y <= 75) {
                pending_snap = WM_SNAP_TOP_RIGHT;
            } else if (mouse_x <= corner_margin && mouse_y >= SCREEN_HEIGHT - 100) {
                pending_snap = WM_SNAP_BOTTOM_LEFT;
            } else if (mouse_x >= SCREEN_WIDTH - corner_margin && mouse_y >= SCREEN_HEIGHT - 100) {
                pending_snap = WM_SNAP_BOTTOM_RIGHT;
            } else if (mouse_y <= 32) {
                pending_snap = WM_SNAP_MAX;
            } else if (mouse_x <= 10) {
                pending_snap = WM_SNAP_LEFT;
            } else if (mouse_x >= SCREEN_WIDTH - 10) {
                pending_snap = WM_SNAP_RIGHT;
            } else {
                pending_snap = WM_SNAP_NONE;
            }
            return;
        } else {
            // Drag released - commit snap if any
            dragging_window->is_dragging = 0;
            dragging_window->opacity = 255;
            window_t* win = dragging_window;
            dragging_window = NULL;

            if (pending_snap == WM_SNAP_MAX) {
                wm_maximize_window(win);
            } else if (pending_snap == WM_SNAP_LEFT) {
                wm_tile_left(win);
            } else if (pending_snap == WM_SNAP_RIGHT) {
                wm_tile_right(win);
            } else if (pending_snap == WM_SNAP_TOP_LEFT) {
                wm_tile_top_left(win);
            } else if (pending_snap == WM_SNAP_TOP_RIGHT) {
                wm_tile_top_right(win);
            } else if (pending_snap == WM_SNAP_BOTTOM_LEFT) {
                wm_tile_bottom_left(win);
            } else if (pending_snap == WM_SNAP_BOTTOM_RIGHT) {
                wm_tile_bottom_right(win);
            } else {
                win->orig_x = win->x;
                win->orig_y = win->y;
                win->orig_w = win->width;
                win->orig_h = win->height;
            }
            pending_snap = WM_SNAP_NONE;
        }
    }

    // 3. Handle click hit testing (front to back)
    if (clicked) {
        for (int i = window_count - 1; i >= 0; i--) {
            window_t* win = window_stack[i];
            if (!win || win->is_closed || win->is_minimized) continue;

            // First: Check resize handle hit
            wm_resize_mode_t rm = wm_hit_test_border(win, mouse_x, mouse_y);
            if (rm != WM_RESIZE_NONE) {
                wm_bring_to_front(win);
                resizing_window = win;
                win->is_resizing = 1;
                win->resize_mode = rm;
                win->resize_start_mx = mouse_x;
                win->resize_start_my = mouse_y;
                win->resize_orig_x = win->x;
                win->resize_orig_y = win->y;
                win->resize_orig_w = win->width;
                win->resize_orig_h = win->height;
                return;
            }

            // Second: Check window body hit
            if (mouse_x >= win->x && mouse_x < win->x + win->width &&
                mouse_y >= win->y && mouse_y < win->y + win->height) {

                wm_bring_to_front(win);

                // Check titlebar hit
                if (mouse_y >= win->y && mouse_y < win->y + WM_TITLEBAR_HEIGHT) {
                    // Check close button (Full outline segment)
                    int close_btn_x = win->x + win->width - 26;
                    if (mouse_x >= close_btn_x && mouse_x <= win->x + win->width) {
                        wm_close_window(win);
                        return;
                    }

                    // Check maximize button (Full outline segment)
                    int max_btn_x = win->x + win->width - 48;
                    if (mouse_x >= max_btn_x && mouse_x < close_btn_x) {
                        wm_maximize_window(win);
                        return;
                    }

                    // Check minimize button (Full outline segment)
                    int min_btn_x = win->x + win->width - 70;
                    if (mouse_x >= min_btn_x && mouse_x < max_btn_x) {
                        wm_minimize_window(win);
                        return;
                    }

                    // Clicked on titlebar body -> start dragging
                    // If snapped or maximized, restore to floating size centered on mouse
                    if (win->is_maximized || win->snap_state != WM_SNAP_NONE) {
                        int old_w = win->orig_w;
                        win->is_maximized = 0;
                        win->snap_state = WM_SNAP_NONE;
                        win->width = old_w;
                        win->height = win->orig_h;
                        win->x = mouse_x - old_w / 2;
                        win->y = mouse_y - 14;
                    }

                    dragging_window = win;
                    win->is_dragging = 1;
                    win->drag_off_x = mouse_x - win->x;
                    win->drag_off_y = mouse_y - win->y;
                    pending_snap = WM_SNAP_NONE;
                    return;
                }

                // Clicked in client area
                if (win->on_mouse) {
                    int rx = mouse_x - (win->x + WM_BORDER_WIDTH);
                    int ry = mouse_y - (win->y + WM_TITLEBAR_HEIGHT);
                    win->on_mouse(win, rx, ry, buttons, WM_EVENT_MOUSE_DOWN);
                    win->on_mouse(win, rx, ry, buttons, WM_EVENT_MOUSE_CLICK);
                }
                return;
            }
        }
    }

    // 4. Mouse move / up / drag inside active window
    if (active_window && !active_window->is_minimized) {
        int rx = mouse_x - (active_window->x + WM_BORDER_WIDTH);
        int ry = mouse_y - (active_window->y + WM_TITLEBAR_HEIGHT);

        if (released && active_window->on_mouse) {
            active_window->on_mouse(active_window, rx, ry, buttons, WM_EVENT_MOUSE_UP);
        } else if ((buttons & 1) && active_window->on_mouse) {
            active_window->on_mouse(active_window, rx, ry, buttons, WM_EVENT_MOUSE_DRAG);
        } else if (active_window->on_mouse) {
            active_window->on_mouse(active_window, rx, ry, buttons, WM_EVENT_MOUSE_MOVE);
        }
    }
}

void wm_handle_key(char key) {
    if (check_and_clear_alt_f4() || (is_alt_pressed() && (key == CHAR_F4 || key == 0x04))) {
        if (active_window && !active_window->is_closed) {
            wm_close_window(active_window);
        }
        return;
    }

    if (switcher_open) {
        if (key == '\t') {
            wm_switcher_next();
            return;
        } else if (key == '\n' || key == ' ') {
            wm_switcher_close();
            return;
        } else if (key == 27) { // Esc
            switcher_open = 0;
            return;
        }
    }

    if (active_window && !active_window->is_minimized) {
        if (active_window->on_key) {
            active_window->on_key(active_window, key);
        }
    }
}

// Draw Aero Snap Glass Highlight Preview Overlay
void wm_draw_snap_preview(void) {
    if (!dragging_window || pending_snap == WM_SNAP_NONE) return;

    int sx = 0, sy = 30, sw = SCREEN_WIDTH, sh = SCREEN_HEIGHT - 30 - 52;
    int avail_h = SCREEN_HEIGHT - 30 - 52;

    switch (pending_snap) {
        case WM_SNAP_LEFT:
            sw = SCREEN_WIDTH / 2;
            break;
        case WM_SNAP_RIGHT:
            sx = SCREEN_WIDTH / 2;
            sw = SCREEN_WIDTH / 2;
            break;
        case WM_SNAP_MAX:
            break;
        case WM_SNAP_TOP_LEFT:
            sw = SCREEN_WIDTH / 2;
            sh = avail_h / 2;
            break;
        case WM_SNAP_TOP_RIGHT:
            sx = SCREEN_WIDTH / 2;
            sw = SCREEN_WIDTH / 2;
            sh = avail_h / 2;
            break;
        case WM_SNAP_BOTTOM_LEFT:
            sy = 30 + avail_h / 2;
            sw = SCREEN_WIDTH / 2;
            sh = avail_h - avail_h / 2;
            break;
        case WM_SNAP_BOTTOM_RIGHT:
            sx = SCREEN_WIDTH / 2;
            sy = 30 + avail_h / 2;
            sw = SCREEN_WIDTH / 2;
            sh = avail_h - avail_h / 2;
            break;
        default:
            return;
    }

    // Outer glow highlight & frosted acrylic preview
    gfx_acrylic_rect(sx + 4, sy + 4, sw - 8, sh - 8, 12, 0x1E1E2E, 130);
    gfx_draw_glow_rect(sx + 4, sy + 4, sw - 8, sh - 8, 12, 0xFF38BDF8, 3);
    gfx_draw_rounded_rect(sx + 4, sy + 4, sw - 8, sh - 8, 12, 0xFF60A5FA);
}

void wm_render_windows(void) {
    for (int i = 0; i < window_count; i++) {
        window_t* win = window_stack[i];
        if (!win || win->is_closed || (win->is_minimized && win->anim_type == WM_ANIM_NONE)) continue;

        int wx = win->x;
        int wy = win->y;
        int ww = win->width;
        int wh = win->height;

        // Smooth fluid animation interpolation
        if (win->anim_type == WM_ANIM_OPENING) {
            int p = win->anim_progress; // 0..100
            int scale_w = (ww * (85 + (p * 15) / 100)) / 100;
            int scale_h = (wh * (85 + (p * 15) / 100)) / 100;
            wx = wx + (ww - scale_w) / 2;
            wy = wy + (wh - scale_h) / 2;
            ww = scale_w;
            wh = scale_h;
        } else if (win->anim_type == WM_ANIM_MINIMIZING) {
            int p = win->anim_progress; // 0..100
            wx = win->anim_start_x + ((win->anim_target_x - win->anim_start_x) * p) / 100;
            wy = win->anim_start_y + ((win->anim_target_y - win->anim_start_y) * p) / 100;
            ww = win->anim_start_w + ((win->anim_target_w - win->anim_start_w) * p) / 100;
            wh = win->anim_start_h + ((win->anim_target_h - win->anim_start_h) * p) / 100;
        } else if (win->anim_type == WM_ANIM_RESTORING) {
            int p = win->anim_progress; // 0..100
            wx = win->anim_start_x + ((win->anim_target_x - win->anim_start_x) * p) / 100;
            wy = win->anim_start_y + ((win->anim_target_y - win->anim_start_y) * p) / 100;
            ww = win->anim_start_w + ((win->anim_target_w - win->anim_start_w) * p) / 100;
            wh = win->anim_start_h + ((win->anim_target_h - win->anim_start_h) * p) / 100;
        }

        // 1. Draw subtle elevation drop shadow (GNOME/Windows style)
        if (!win->is_maximized && win->snap_state == WM_SNAP_NONE) {
            gfx_draw_shadow(wx, wy, ww, wh, win->is_active ? 10 : 6, win->is_active ? 150 : 90);
        }

        // 2. Draw window container background
        gfx_fill_rounded_rect(wx, wy, ww, wh, 8, COLOR_SURFACE);

        // 3. Draw Titlebar (Active vs Inactive)
        if (win->is_active) {
            // Elegant illuminated gradient for active titlebar
            gfx_draw_gradient_v(wx + 1, wy + 1, ww - 2, WM_TITLEBAR_HEIGHT - 1, 0xFF32344B, 0xFF1E1E2E);
            // Window border outline with ambient glow
            gfx_draw_rounded_rect(wx, wy, ww, wh, 8, 0xFF60A5FA);
            if (win->is_dragging) {
                gfx_draw_glow_rect(wx, wy, ww, wh, 8, 0xFF38BDF8, 3);
            }
            // Specular glass shine across the top edge
            for (int px = wx + 8; px < wx + ww - 8; px++) {
                gfx_put_pixel_alpha(px, wy + 1, 0xFFFFFF, 85);
            }
        } else {
            // Darker subtle titlebar for inactive windows
            gfx_fill_rect(wx + 1, wy + 1, ww - 2, WM_TITLEBAR_HEIGHT - 1, 0xFF181825);
            gfx_draw_rounded_rect(wx, wy, ww, wh, 8, COLOR_BORDER);
        }

        // Titlebar separator line
        gfx_fill_rect(wx, wy + WM_TITLEBAR_HEIGHT - 1, ww, 1, COLOR_OVERLAY);

        // 4. Window Icon & Title Text
        int title_x = wx + 10;
        if (win->icon_id) {
            icon_draw_16(title_x, wy + 8, (icon_type_t)win->icon_id, COLOR_TEXT_PRIMARY);
            title_x += 20;
        }

        uint32_t title_color = win->is_active ? COLOR_TEXT_PRIMARY : COLOR_TEXT_MUTED;
        int max_title_w = (wx + ww - 70) - title_x;
        if (max_title_w > 10) {
            gfx_draw_text_ellipsis(title_x, wy + 8, max_title_w, win->title, title_color);
        }

        // 5. Titlebar Control Buttons (GNOME/Windows macOS style pills)
        int btn_y = wy + 10;

        // Close button (Red circle)
        int close_x = wx + ww - 20;
        gfx_fill_rounded_rect(close_x - 3, btn_y - 2, 16, 16, 8, COLOR_RED_ACCENT);
        gfx_draw_icon(close_x - 3, btn_y - 2, ICON_CLOSE, 0xFF11111B);

        // Maximize/Restore button (Green circle)
        int max_x = wx + ww - 42;
        gfx_fill_rounded_rect(max_x - 3, btn_y - 2, 16, 16, 8, COLOR_GREEN_ACCENT);
        gfx_draw_icon(max_x - 3, btn_y - 2, (win->is_maximized || win->snap_state != WM_SNAP_NONE) ? ICON_RESTORE : ICON_MAXIMIZE, 0xFF11111B);

        // Minimize button (Yellow circle)
        int min_x = wx + ww - 64;
        gfx_fill_rounded_rect(min_x - 3, btn_y - 2, 16, 16, 8, COLOR_YELLOW_ACCENT);
        gfx_draw_icon(min_x - 3, btn_y - 2, ICON_MINIMIZE, 0xFF11111B);

        // 6. Window Client Area
        int cx = wx + WM_BORDER_WIDTH;
        int cy = wy + WM_TITLEBAR_HEIGHT;
        int cw = ww - 2 * WM_BORDER_WIDTH;
        int ch = wh - WM_TITLEBAR_HEIGHT - WM_BORDER_WIDTH;

        // Clip strictly to client area so window contents never spill over
        gfx_set_clip(cx, cy, cw, ch);

        if (win->render) {
            win->render(win, cx, cy, cw, ch);
        } else {
            gfx_fill_rect(cx, cy, cw, ch, COLOR_SURFACE_LIGHT);
        }

        gfx_reset_clip();

        // 7. Visual Resize Grip for floating windows at bottom-right corner
        if (!win->is_maximized && win->snap_state == WM_SNAP_NONE && win->is_active) {
            int gx = wx + ww - 10;
            int gy = wy + wh - 10;
            gfx_fill_rect(gx + 4, gy + 4, 2, 2, COLOR_TEXT_MUTED);
            gfx_fill_rect(gx,     gy + 4, 2, 2, COLOR_TEXT_MUTED);
            gfx_fill_rect(gx + 4, gy,     2, 2, COLOR_TEXT_MUTED);
        }
    }
}

// Alt-Tab Window Switcher HUD
void wm_switcher_open(void) {
    if (window_count <= 0) return;
    switcher_open = 1;
    switcher_selected_idx = 0;
}

void wm_switcher_next(void) {
    if (!switcher_open || window_count <= 0) return;
    switcher_selected_idx = (switcher_selected_idx + 1) % window_count;
}

void wm_switcher_close(void) {
    if (!switcher_open) return;
    switcher_open = 0;
    if (switcher_selected_idx >= 0 && switcher_selected_idx < window_count) {
        window_t* win = window_stack[switcher_selected_idx];
        if (win && !win->is_closed) {
            if (win->is_minimized) wm_restore_window(win);
            wm_bring_to_front(win);
        }
    }
}

int wm_switcher_is_open(void) {
    return switcher_open;
}

void wm_switcher_select_index(int idx) {
    if (idx >= 0 && idx < window_count) {
        switcher_selected_idx = idx;
    }
}

void wm_draw_switcher(void) {
    if (!switcher_open || window_count <= 0) return;

    int item_w = 90;
    int item_h = 80;
    int gap = 12;
    int total_w = window_count * item_w + (window_count - 1) * gap + 32;
    if (total_w > SCREEN_WIDTH - 60) total_w = SCREEN_WIDTH - 60;
    int total_h = item_h + 40;

    int sx = (SCREEN_WIDTH - total_w) / 2;
    int sy = (SCREEN_HEIGHT - total_h) / 2;

    // Drop shadow & Acrylic card
    gfx_draw_shadow(sx, sy, total_w, total_h, 12, 160);
    gfx_fill_rounded_rect(sx, sy, total_w, total_h, 14, 0xFA181825);
    gfx_draw_rounded_rect(sx, sy, total_w, total_h, 14, COLOR_BORDER_LIGHT);

    gfx_draw_text(sx + 16, sy + 10, "Window Switcher (Press Tab / Enter)", COLOR_TEXT_MUTED);

    for (int i = 0; i < window_count; i++) {
        window_t* win = window_stack[i];
        if (!win || win->is_closed) continue;

        int ix = sx + 16 + i * (item_w + gap);
        int iy = sy + 30;
        int is_sel = (i == switcher_selected_idx);

        uint32_t bg = is_sel ? 0xFF2A2B3D : 0xFF1E1E2E;
        gfx_fill_rounded_rect(ix, iy, item_w, item_h, 8, bg);
        if (is_sel) {
            gfx_draw_rounded_rect(ix, iy, item_w, item_h, 8, COLOR_LEMON_GOLD);
        } else {
            gfx_draw_rounded_rect(ix, iy, item_w, item_h, 8, COLOR_OVERLAY);
        }

        // Draw Icon centered
        icon_draw_32(ix + (item_w - 32) / 2, iy + 10, (icon_type_t)win->icon_id);

        // Responsive title with ellipsis
        gfx_draw_text_ellipsis(ix + 6, iy + 52, item_w - 12, win->title, is_sel ? COLOR_TEXT_PRIMARY : COLOR_TEXT_SECONDARY);
    }
}

int wm_get_window_count(void) {
    return window_count;
}

window_t* wm_get_window(int index) {
    if (index >= 0 && index < window_count) {
        return window_stack[index];
    }
    return NULL;
}

// Mission Control / Window Overview Mode
static int overview_open = 0;

void wm_overview_open(void) {
    if (window_count <= 0) return;
    overview_open = 1;
}

void wm_overview_close(void) {
    overview_open = 0;
}

void wm_overview_toggle(void) {
    if (overview_open) wm_overview_close();
    else wm_overview_open();
}

int wm_overview_is_open(void) {
    return overview_open;
}

void wm_draw_overview(int mouse_x, int mouse_y) {
    if (!overview_open || window_count <= 0) return;

    // 1. Dim background with soft acrylic blur
    gfx_fill_rect_alpha(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0x0A0B14, 185);
    gfx_blur_rect(40, 40, SCREEN_WIDTH - 80, SCREEN_HEIGHT - 80, 2);

    // 2. Top Title Pill
    int pill_w = 400;
    int pill_h = 40;
    int pill_x = (SCREEN_WIDTH - pill_w) / 2;
    int pill_y = 44;
    gfx_draw_shadow(pill_x, pill_y, pill_w, pill_h, 8, 140);
    gfx_acrylic_rect(pill_x, pill_y, pill_w, pill_h, 20, 0x1E1E2E, 230);
    gfx_draw_glow_rect(pill_x, pill_y, pill_w, pill_h, 20, 0xFF38BDF8, 2);
    icon_draw_16(pill_x + 18, pill_y + 12, ICON_RESTORE, COLOR_LEMON_GOLD);
    gfx_draw_text(pill_x + 44, pill_y + 12, "Mission Control - Running Windows", COLOR_TEXT_PRIMARY);

    // 3. Collect active non-closed windows
    int visible_count = 0;
    window_t* visible_wins[MAX_WINDOWS];
    for (int i = 0; i < window_count; i++) {
        if (window_stack[i] && !window_stack[i]->is_closed) {
            visible_wins[visible_count++] = window_stack[i];
        }
    }
    if (visible_count == 0) return;

    // 4. Compute Grid Geometry
    int cols = (visible_count <= 2) ? visible_count : (visible_count <= 4 ? 2 : (visible_count <= 6 ? 3 : 4));
    if (cols < 1) cols = 1;
    int card_w = (SCREEN_WIDTH - 120 - (cols - 1) * 24) / cols;
    if (card_w > 320) card_w = 320;
    int card_h = 210;
    int grid_total_w = cols * card_w + (cols - 1) * 24;
    int start_x = (SCREEN_WIDTH - grid_total_w) / 2;
    int start_y = 110;

    for (int idx = 0; idx < visible_count; idx++) {
        window_t* win = visible_wins[idx];
        int r = idx / cols;
        int c = idx % cols;
        int cx = start_x + c * (card_w + 24);
        int cy = start_y + r * (card_h + 24);

        int is_hover = (mouse_x >= cx && mouse_x < cx + card_w && mouse_y >= cy && mouse_y < cy + card_h);

        // Elevation Drop Shadow
        gfx_draw_shadow(cx, cy, card_w, card_h, is_hover ? 14 : 8, is_hover ? 190 : 120);

        // Container Surface with Acrylic Glass
        gfx_acrylic_rect(cx, cy, card_w, card_h, 12, win->is_active ? 0x2A2C40 : 0x1E1E2E, 235);
        if (is_hover) {
            gfx_draw_glow_rect(cx, cy, card_w, card_h, 12, 0xFF38BDF8, 3);
            gfx_draw_rounded_rect(cx, cy, card_w, card_h, 12, 0xFF60A5FA);
        } else if (win->is_active) {
            gfx_draw_rounded_rect(cx, cy, card_w, card_h, 12, COLOR_BORDER_LIGHT);
        }

        // Titlebar
        icon_draw_16(cx + 12, cy + 10, (icon_type_t)win->icon_id, COLOR_TEXT_PRIMARY);
        gfx_draw_text_ellipsis(cx + 34, cy + 10, card_w - 70, win->title, is_hover ? COLOR_LEMON_GOLD : COLOR_TEXT_PRIMARY);

        // Close button on card top-right
        int cb_x = cx + card_w - 24;
        int cb_y = cy + 8;
        int cb_hover = (mouse_x >= cb_x - 2 && mouse_x < cb_x + 18 && mouse_y >= cb_y - 2 && mouse_y < cb_y + 18);
        gfx_fill_rounded_rect(cb_x, cb_y, 16, 16, 8, cb_hover ? 0xFFEF4444 : COLOR_RED_ACCENT);
        gfx_draw_icon(cb_x, cb_y, ICON_CLOSE, 0xFF11111B);

        // Thumbnail Viewport
        int prev_x = cx + 8;
        int prev_y = cy + 32;
        int prev_w = card_w - 16;
        int prev_h = card_h - 40;
        gfx_fill_rounded_rect(prev_x, prev_y, prev_w, prev_h, 8, 0xFF11111B);
        gfx_draw_rounded_rect(prev_x, prev_y, prev_w, prev_h, 8, COLOR_OVERLAY);

        // Stylized window simulation lines
        gfx_fill_rounded_rect(prev_x + 6, prev_y + 6, prev_w - 12, 10, 3, 0xFF1E1E2E);
        gfx_fill_rounded_rect(prev_x + 10, prev_y + 9, 4, 4, 2, COLOR_RED_ACCENT);
        gfx_fill_rounded_rect(prev_x + 16, prev_y + 9, 4, 4, 2, COLOR_YELLOW_ACCENT);
        gfx_fill_rounded_rect(prev_x + 22, prev_y + 9, 4, 4, 2, COLOR_GREEN_ACCENT);

        for (int l = 0; l < 6; l++) {
            int lw = (prev_w - 24) * ((l % 2 == 0) ? 8 : 6) / 10;
            gfx_fill_rounded_rect(prev_x + 12, prev_y + 24 + l * 14, lw, 6, 2, 0xFF2A2B3D);
        }

        // Status badge
        if (win->is_active) {
            gfx_fill_rounded_rect(prev_x + prev_w - 58, prev_y + prev_h - 18, 52, 14, 4, 0xFF2563EB);
            gfx_draw_text(prev_x + prev_w - 54, prev_y + prev_h - 16, "ACTIVE", 0xFFFFFFFF);
        } else if (win->is_minimized) {
            gfx_fill_rounded_rect(prev_x + prev_w - 68, prev_y + prev_h - 18, 62, 14, 4, 0xFF45475A);
            gfx_draw_text(prev_x + prev_w - 64, prev_y + prev_h - 16, "MINIMIZED", COLOR_TEXT_MUTED);
        }
    }
}

int wm_overview_handle_mouse(int mouse_x, int mouse_y, int clicked) {
    if (!overview_open) return 0;
    if (!clicked) return 1;

    int visible_count = 0;
    window_t* visible_wins[MAX_WINDOWS];
    for (int i = 0; i < window_count; i++) {
        if (window_stack[i] && !window_stack[i]->is_closed) {
            visible_wins[visible_count++] = window_stack[i];
        }
    }

    if (visible_count == 0) {
        wm_overview_close();
        return 1;
    }

    int cols = (visible_count <= 2) ? visible_count : (visible_count <= 4 ? 2 : (visible_count <= 6 ? 3 : 4));
    if (cols < 1) cols = 1;
    int card_w = (SCREEN_WIDTH - 120 - (cols - 1) * 24) / cols;
    if (card_w > 320) card_w = 320;
    int card_h = 210;
    int grid_total_w = cols * card_w + (cols - 1) * 24;
    int start_x = (SCREEN_WIDTH - grid_total_w) / 2;
    int start_y = 110;

    for (int idx = 0; idx < visible_count; idx++) {
        window_t* win = visible_wins[idx];
        int r = idx / cols;
        int c = idx % cols;
        int cx = start_x + c * (card_w + 24);
        int cy = start_y + r * (card_h + 24);

        if (mouse_x >= cx && mouse_x < cx + card_w && mouse_y >= cy && mouse_y < cy + card_h) {
            int cb_x = cx + card_w - 24;
            int cb_y = cy + 8;
            if (mouse_x >= cb_x - 2 && mouse_x < cb_x + 18 && mouse_y >= cb_y - 2 && mouse_y < cb_y + 18) {
                wm_destroy_window(win);
                if (window_count == 0) wm_overview_close();
                return 1;
            }

            if (win->is_minimized) wm_restore_window(win);
            wm_bring_to_front(win);
            wm_overview_close();
            return 1;
        }
    }

    // Clicked outside cards -> close overview
    wm_overview_close();
    return 1;
}
