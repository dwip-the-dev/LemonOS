#include "gui_breakout.h"
#include "graphics.h"
#include "font.h"
#include <stdint.h>
#include <stddef.h>

#define BO_ROWS 5
#define BO_COLS 8

typedef struct {
    int paddle_x;
    int paddle_w;
    int ball_x, ball_y;
    int ball_dx, ball_dy;
    int bricks[BO_ROWS][BO_COLS];
    int score;
    int lives;
    int bricks_left;
    int game_over;
    int game_won;
} breakout_state_t;

static breakout_state_t bo_state;
static window_t* bo_win = NULL;

static const uint32_t brick_row_colors[BO_ROWS] = {
    COLOR_RED_ACCENT,
    COLOR_LEMON_AMBER,
    COLOR_GREEN_ACCENT,
    COLOR_BLUE_ACCENT,
    COLOR_PURPLE_ACCENT
};

static void bo_reset(void) {
    bo_state.paddle_w = 64;
    bo_state.paddle_x = 180;
    bo_state.ball_x = 210;
    bo_state.ball_y = 260;
    bo_state.ball_dx = 3;
    bo_state.ball_dy = -3;
    bo_state.score = 0;
    bo_state.lives = 3;
    bo_state.bricks_left = BO_ROWS * BO_COLS;
    bo_state.game_over = 0;
    bo_state.game_won = 0;

    for (int r = 0; r < BO_ROWS; r++) {
        for (int c = 0; c < BO_COLS; c++) {
            bo_state.bricks[r][c] = 1;
        }
    }
}

static void bo_render(window_t* win, int cx, int cy, int cw, int ch) {
    (void)win; (void)ch;
    // Dark arcade background
    gfx_fill_rect(cx, cy, cw, ch, 0xFF11111B);

    // 1. Header Bar: Score, Lives & Instructions
    gfx_fill_rect(cx, cy, cw, 26, 0xFF181825);
    gfx_fill_rect(cx, cy + 25, cw, 1, COLOR_OVERLAY);

    char score_buf[24];
    score_buf[0] = 'S'; score_buf[1] = 'c'; score_buf[2] = 'o'; score_buf[3] = 'r'; score_buf[4] = 'e'; score_buf[5] = ':'; score_buf[6] = ' ';
    int sp = 7;
    int sc = bo_state.score;
    if (sc == 0) score_buf[sp++] = '0';
    else {
        char temp[16]; int tp = 0;
        while (sc > 0) { temp[tp++] = '0' + (sc % 10); sc /= 10; }
        while (tp > 0) score_buf[sp++] = temp[--tp];
    }
    score_buf[sp] = '\0';
    gfx_draw_text(cx + 12, cy + 5, score_buf, COLOR_LEMON_GOLD);

    // Draw Lives
    gfx_draw_text(cx + 140, cy + 5, "Lives:", COLOR_TEXT_MUTED);
    for (int i = 0; i < bo_state.lives; i++) {
        gfx_fill_rounded_rect(cx + 195 + i * 16, cy + 8, 10, 10, 4, COLOR_RED_ACCENT);
    }

    gfx_draw_text(cx + cw - 170, cy + 5, "Move Mouse / A,D to Play", COLOR_TEXT_MUTED);

    // 2. Physics & State Update
    if (!bo_state.game_over && !bo_state.game_won) {
        bo_state.ball_x += bo_state.ball_dx;
        bo_state.ball_y += bo_state.ball_dy;

        // Left / Right wall bounce
        if (bo_state.ball_x <= cx + 6) {
            bo_state.ball_x = cx + 6;
            bo_state.ball_dx = -bo_state.ball_dx;
        } else if (bo_state.ball_x >= cx + cw - 12) {
            bo_state.ball_x = cx + cw - 12;
            bo_state.ball_dx = -bo_state.ball_dx;
        }

        // Top wall bounce
        if (bo_state.ball_y <= cy + 30) {
            bo_state.ball_y = cy + 30;
            bo_state.ball_dy = -bo_state.ball_dy;
        }

        // Paddle collision
        int paddle_y = cy + ch - 22;
        int px = cx + bo_state.paddle_x;
        if (bo_state.ball_y >= paddle_y - 6 && bo_state.ball_y <= paddle_y + 4 &&
            bo_state.ball_x >= px - 4 && bo_state.ball_x <= px + bo_state.paddle_w + 4) {
            bo_state.ball_dy = -bo_state.ball_dy;
            bo_state.ball_y = paddle_y - 7;

            // English / spin based on where ball hits paddle
            int hit = (bo_state.ball_x - px) - bo_state.paddle_w / 2;
            bo_state.ball_dx = hit / 8;
            if (bo_state.ball_dx == 0) bo_state.ball_dx = (hit < 0) ? -2 : 2;
        }

        // Brick collision
        int brick_margin_x = 16;
        int brick_gap = 6;
        int brick_w = (cw - 2 * brick_margin_x - (BO_COLS - 1) * brick_gap) / BO_COLS;
        int brick_h = 14;
        int brick_start_y = cy + 38;

        for (int r = 0; r < BO_ROWS; r++) {
            for (int c = 0; c < BO_COLS; c++) {
                if (bo_state.bricks[r][c]) {
                    int bx = cx + brick_margin_x + c * (brick_w + brick_gap);
                    int by = brick_start_y + r * (brick_h + brick_gap);

                    if (bo_state.ball_x >= bx && bo_state.ball_x <= bx + brick_w &&
                        bo_state.ball_y >= by && bo_state.ball_y <= by + brick_h) {
                        bo_state.bricks[r][c] = 0;
                        bo_state.bricks_left--;
                        bo_state.score += 15;
                        bo_state.ball_dy = -bo_state.ball_dy;

                        if (bo_state.bricks_left == 0) {
                            bo_state.game_won = 1;
                        }
                        goto after_collision;
                    }
                }
            }
        }
    after_collision:

        // Bottom death
        if (bo_state.ball_y >= cy + ch - 8) {
            bo_state.lives--;
            if (bo_state.lives > 0) {
                bo_state.ball_x = cx + bo_state.paddle_x + bo_state.paddle_w / 2;
                bo_state.ball_y = cy + ch - 36;
                bo_state.ball_dy = -3;
                bo_state.ball_dx = 3;
            } else {
                bo_state.game_over = 1;
            }
        }
    }

    // 3. Render Bricks
    int brick_margin_x = 16;
    int brick_gap = 6;
    int brick_w = (cw - 2 * brick_margin_x - (BO_COLS - 1) * brick_gap) / BO_COLS;
    int brick_h = 14;
    int brick_start_y = cy + 38;

    for (int r = 0; r < BO_ROWS; r++) {
        for (int c = 0; c < BO_COLS; c++) {
            if (bo_state.bricks[r][c]) {
                int bx = cx + brick_margin_x + c * (brick_w + brick_gap);
                int by = brick_start_y + r * (brick_h + brick_gap);
                gfx_fill_rounded_rect(bx, by, brick_w, brick_h, 3, brick_row_colors[r]);
                gfx_draw_rect(bx, by, brick_w, 2, 0xFFFFFFFF); // Highlight line
            }
        }
    }

    // 4. Render Paddle (Smooth Cyan/Lemon rounded capsule)
    int paddle_y = cy + ch - 22;
    int px = cx + bo_state.paddle_x;
    gfx_fill_rounded_rect(px, paddle_y, bo_state.paddle_w, 10, 5, COLOR_BLUE_ACCENT);
    gfx_fill_rounded_rect(px + 4, paddle_y + 2, bo_state.paddle_w - 8, 3, 2, 0xFFFFFFFF);

    // 5. Render Ball (Crisp glowing sphere)
    gfx_fill_rounded_rect(bo_state.ball_x - 4, bo_state.ball_y - 4, 8, 8, 4, 0xFFFFFFFF);
    gfx_put_pixel(bo_state.ball_x - 1, bo_state.ball_y - 1, 0xFFF9E2AF);

    // 6. Overlay: Game Over or Win
    if (bo_state.game_over || bo_state.game_won) {
        int ov_w = 240;
        int ov_h = 74;
        int ov_x = cx + (cw - ov_w) / 2;
        int ov_y = cy + (ch - ov_h) / 2;
        gfx_fill_rounded_rect(ov_x, ov_y, ov_w, ov_h, 8, 0xFF11111B);
        gfx_draw_rounded_rect(ov_x, ov_y, ov_w, ov_h, 8, bo_state.game_won ? COLOR_GREEN_ACCENT : COLOR_RED_ACCENT);

        const char* msg = bo_state.game_won ? "VICTORY! YOU WIN!" : "GAME OVER";
        uint32_t col = bo_state.game_won ? COLOR_GREEN_ACCENT : COLOR_RED_ACCENT;
        int mw = font_get_string_width(msg);
        gfx_draw_text(ov_x + (ov_w - mw) / 2, ov_y + 14, msg, col);
        int rw = font_get_string_width("Click or Press Space to Play Again");
        gfx_draw_text(ov_x + (ov_w - rw) / 2, ov_y + 40, "Click to Play Again", COLOR_TEXT_PRIMARY);
    }
}

static void bo_on_mouse(window_t* win, int rx, int ry, int buttons, int event_type) {
    (void)ry; (void)buttons;
    if ((event_type == WM_EVENT_MOUSE_CLICK) && (bo_state.game_over || bo_state.game_won)) {
        bo_reset();
        return;
    }

    // Move paddle with mouse
    int target_x = rx - bo_state.paddle_w / 2;
    int max_x = win->width - 6 - bo_state.paddle_w - 6;
    if (target_x < 6) target_x = 6;
    if (target_x > max_x) target_x = max_x;
    bo_state.paddle_x = target_x;
}

static void bo_on_key(window_t* win, char key) {
    (void)win;
    if (bo_state.game_over || bo_state.game_won) {
        if (key == ' ' || key == '\n') bo_reset();
        return;
    }

    if (key == 'a' || key == 'A' || key == 0x13) {
        bo_state.paddle_x -= 16;
        if (bo_state.paddle_x < 6) bo_state.paddle_x = 6;
    } else if (key == 'd' || key == 'D' || key == 0x14) {
        bo_state.paddle_x += 16;
        int max_x = win->width - 6 - bo_state.paddle_w - 6;
        if (bo_state.paddle_x > max_x) bo_state.paddle_x = max_x;
    }
}

static void bo_on_close(window_t* win) {
    (void)win;
    bo_win = NULL;
}

void gui_breakout_open(void) {
    if (bo_win && !bo_win->is_closed) {
        if (bo_win->is_minimized) wm_restore_window(bo_win);
        wm_bring_to_front(bo_win);
        return;
    }

    bo_reset();
    bo_win = wm_create_window("Breakout (Brick Breaker)", ICON_GAMES, 160, 90, 460, 360, bo_render);
    if (bo_win) {
        bo_win->on_mouse = bo_on_mouse;
        bo_win->on_key = bo_on_key;
        bo_win->on_close = bo_on_close;
    }
}
