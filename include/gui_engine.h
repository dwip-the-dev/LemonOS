#ifndef GUI_ENGINE_H
#define GUI_ENGINE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Hitbox rectangle representation
typedef struct {
    int x;
    int y;
    int w;
    int h;
} gui_rect_t;

// Hitbox testing: returns 1 if (px, py) is strictly inside the outline box [x, y, w, h]
int gui_hitbox_contains(int x, int y, int w, int h, int px, int py);

// Render an interactive card with outline box, rounded corners, and hover feedback
void gui_render_card(int x, int y, int w, int h, int radius, uint32_t bg_color, uint32_t border_color, int is_hover);

// Render an interactive button with outline box and full hitbox clickability
void gui_render_button(int x, int y, int w, int h, const char* label, uint32_t bg_color, uint32_t fg_color, int is_hover);

// Render text clipped to max_width with automatic ellipsis "..." if overflow occurs
void gui_render_text_ellipsis(int x, int y, int max_w, const char* text, uint32_t color);

// Render text strictly clipped within rectangle
void gui_render_text_clipped(int x, int y, int max_w, const char* text, uint32_t color);

// Desktop icon tile geometry: gets the full clickable outline box for a desktop icon
void gui_get_desktop_icon_box(int ix, int iy, gui_rect_t* out_box);

// Desktop icon rendering with full outline tile hover and label ellipsis
void gui_render_desktop_tile(int ix, int iy, const char* label, int icon_id, int is_hover);

#ifdef __cplusplus
}

// C++ Object-Oriented Component API
class GuiBox {
public:
    int x;
    int y;
    int width;
    int height;

    GuiBox(int x_ = 0, int y_ = 0, int w_ = 0, int h_ = 0)
        : x(x_), y(y_), width(w_), height(h_) {}

    inline bool contains(int px, int py) const {
        return (px >= x && px < x + width && py >= y && py < y + height);
    }

    void render_card(int radius, uint32_t bg, uint32_t border, bool is_hover) const;
    void render_button(const char* label, uint32_t bg, uint32_t fg, bool is_hover) const;
};

class GuiLayout {
public:
    static void clip_text_ellipsis(int x, int y, int max_w, const char* text, uint32_t color);
    static void clip_text(int x, int y, int max_w, const char* text, uint32_t color);
};

#endif

#endif // GUI_ENGINE_H
