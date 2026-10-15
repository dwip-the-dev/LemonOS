#include "alerts.h"
#include "graphics.h"
#include "font.h"
#include "icons.h"
#include "string.h"
#include <stdint.h>
#include <stddef.h>

static alert_t active_alerts[MAX_ACTIVE_ALERTS];
static alert_t alert_history[MAX_ALERT_HISTORY];
static int history_count = 0;
static int unread_count = 0;
static int next_alert_id = 1;

void alert_init(void) {
    for (int i = 0; i < MAX_ACTIVE_ALERTS; i++) {
        active_alerts[i].is_active = 0;
        active_alerts[i].id = 0;
    }
    for (int i = 0; i < MAX_ALERT_HISTORY; i++) {
        alert_history[i].is_active = 0;
    }
    history_count = 0;
    unread_count = 0;
    next_alert_id = 1;
}

int alert_send(const char* title, const char* message, int icon_id, alert_type_t type) {
    if (!title || !message) return -1;

    // Find free slot or steal oldest slot
    int slot = -1;
    for (int i = 0; i < MAX_ACTIVE_ALERTS; i++) {
        if (!active_alerts[i].is_active) {
            slot = i;
            break;
        }
    }
    if (slot == -1) {
        // Steal slot with smallest remaining frames
        int min_rem = 999999;
        for (int i = 0; i < MAX_ACTIVE_ALERTS; i++) {
            if (active_alerts[i].remaining_frames < min_rem) {
                min_rem = active_alerts[i].remaining_frames;
                slot = i;
            }
        }
    }
    if (slot == -1) slot = 0;

    alert_t* a = &active_alerts[slot];
    a->id = next_alert_id++;
    a->type = type;
    a->icon_id = (icon_id > 0) ? icon_id : ICON_INFO;
    a->duration_frames = 280; // ~6.5 seconds at 40-50 fps
    a->remaining_frames = a->duration_frames;
    a->is_active = 1;

    // Start offscreen to the right and slide left
    a->anim_x = SCREEN_WIDTH + 20;
    a->target_x = SCREEN_WIDTH - ALERT_WIDTH - 16;
    a->timestamp_sec = 0;

    // Copy title
    int i = 0;
    while (title[i] && i < 47) {
        a->title[i] = title[i];
        i++;
    }
    a->title[i] = '\0';

    // Copy message
    i = 0;
    while (message[i] && i < 95) {
        a->message[i] = message[i];
        i++;
    }
    a->message[i] = '\0';

    // Add to history ring buffer (shift if full)
    if (history_count < MAX_ALERT_HISTORY) {
        alert_history[history_count++] = *a;
    } else {
        for (int h = 0; h < MAX_ALERT_HISTORY - 1; h++) {
            alert_history[h] = alert_history[h + 1];
        }
        alert_history[MAX_ALERT_HISTORY - 1] = *a;
    }
    unread_count++;

    return a->id;
}

void alert_update(void) {
    for (int i = 0; i < MAX_ACTIVE_ALERTS; i++) {
        alert_t* a = &active_alerts[i];
        if (!a->is_active) continue;

        // Smooth horizontal slide ease
        int diff = a->target_x - a->anim_x;
        if (diff != 0) {
            int step = diff / 3;
            if (step == 0) step = (diff > 0) ? 1 : -1;
            a->anim_x += step;
        }

        // Countdown timer
        if (a->remaining_frames > 0) {
            a->remaining_frames--;
            if (a->remaining_frames == 0) {
                // Slide out to the right
                a->target_x = SCREEN_WIDTH + 30;
            }
        } else {
            // Sliding out: once off-screen, mark inactive
            if (a->anim_x >= SCREEN_WIDTH + 20) {
                a->is_active = 0;
            }
        }
    }
}

void alert_render(void) {
    for (int i = 0; i < MAX_ACTIVE_ALERTS; i++) {
        alert_t* a = &active_alerts[i];
        if (!a->is_active) continue;

        int ax = a->anim_x;
        int ay = 36 + i * (ALERT_HEIGHT + 8);
        int aw = ALERT_WIDTH;
        int ah = ALERT_HEIGHT;

        // If off-screen, skip render
        if (ax >= SCREEN_WIDTH) continue;

        // 1. Elevation Drop Shadow
        gfx_draw_shadow(ax, ay, aw, ah, 8, 110);

        // 2. Frosted Acrylic Surface
        gfx_fill_rounded_rect(ax, ay, aw, ah, 10, 0xEE1E1E2E);
        gfx_draw_rounded_rect(ax, ay, aw, ah, 10, COLOR_BORDER_LIGHT);

        // 3. Left Type Stripe
        uint32_t type_color = 0xFF38BDF8; // Info blue
        if (a->type == ALERT_TYPE_SUCCESS) type_color = 0xFF34D399; // Green
        else if (a->type == ALERT_TYPE_WARNING) type_color = 0xFFFBBF24; // Amber
        else if (a->type == ALERT_TYPE_ERROR) type_color = 0xFFF87171; // Red

        gfx_fill_rounded_rect(ax + 3, ay + 3, 4, ah - 6, 2, type_color);

        // 4. Icon Badge
        int icon_bg_size = 28;
        int ibx = ax + 14;
        int iby = ay + (ah - icon_bg_size) / 2 - 2;
        gfx_fill_rounded_rect(ibx, iby, icon_bg_size, icon_bg_size, icon_bg_size / 2, 0x33000000 | (type_color & 0x00FFFFFF));
        icon_draw_16(ibx + 6, iby + 6, (icon_type_t)a->icon_id, type_color);

        // 5. Title & Message Text with responsive bounds checking
        int tx = ax + 50;
        int ty = ay + 12;
        int avail_w = aw - (tx - ax) - 24;
        gfx_draw_text_ellipsis(tx, ty, avail_w, a->title, COLOR_TEXT_PRIMARY);
        gfx_draw_text_ellipsis(tx, ty + 18, avail_w, a->message, COLOR_TEXT_SECONDARY);

        // 6. Dismiss (X) button at top right
        int close_x = ax + aw - 18;
        int close_y = ay + 8;
        gfx_fill_rounded_rect(close_x - 2, close_y - 2, 14, 14, 7, 0x33FFFFFF);
        gfx_draw_icon(close_x - 2, close_y - 2, ICON_CLOSE, COLOR_TEXT_MUTED);

        // 7. Auto-dismiss progress countdown bar
        if (a->duration_frames > 0 && a->remaining_frames > 0) {
            int bar_x = tx;
            int bar_y = ay + ah - 8;
            int bar_w = aw - (tx - ax) - 14;
            int fill_w = (bar_w * a->remaining_frames) / a->duration_frames;
            if (fill_w < 0) fill_w = 0;

            // Background line
            gfx_fill_rect(bar_x, bar_y, bar_w, 2, 0xFF313244);
            // Filled line
            if (fill_w > 0) {
                gfx_fill_rect(bar_x, bar_y, fill_w, 2, type_color);
            }
        }
    }
}

int alert_handle_mouse(int mx, int my, int clicked) {
    int handled = 0;
    for (int i = 0; i < MAX_ACTIVE_ALERTS; i++) {
        alert_t* a = &active_alerts[i];
        if (!a->is_active) continue;

        int ax = a->anim_x;
        int ay = 36 + i * (ALERT_HEIGHT + 8);
        int aw = ALERT_WIDTH;
        int ah = ALERT_HEIGHT;

        if (mx >= ax && mx < ax + aw && my >= ay && my < ay + ah) {
            handled = 1;
            // Pause countdown while hovering
            if (a->remaining_frames > 15) {
                a->remaining_frames = 180;
            }

            // Click anywhere on toast or close button to dismiss
            if (clicked) {
                a->remaining_frames = 0;
                a->target_x = SCREEN_WIDTH + 30;
            }
            break;
        }
    }
    return handled;
}

int alert_get_history_count(void) {
    return history_count;
}

alert_t* alert_get_history(int index) {
    if (index >= 0 && index < history_count) {
        return &alert_history[index];
    }
    return NULL;
}

void alert_clear_history(void) {
    history_count = 0;
    unread_count = 0;
}

int alert_get_unread_count(void) {
    return unread_count;
}

void alert_mark_all_read(void) {
    unread_count = 0;
}
