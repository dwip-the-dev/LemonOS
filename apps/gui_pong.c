#include "gui_pong.h"
#include "graphics.h"
#include "font.h"
#include <stdint.h>
#include <stddef.h>

typedef struct {
    int left_paddle_y;
    int right_paddle_y;
    int paddle_h;
    int paddle_w;
    int ball_x, ball_y;
    int ball_dx, ball_dy;
    int left_score;
    int right_score;
    int game_over;
} pong_state_t;

static pong_state_t pong_state;
static window_t* pong_win = NULL;

static void pong_reset(void) {
    pong_state.paddle_h = 50;
    pong_state.paddle_w = 8;
    pong_state.left_paddle_y = 110;
    pong_state.right_paddle_y = 110;
    pong_state.ball_x = 210;
    pong_state.ball_y = 130;
    pong_state.ball_dx = 3;
    pong_state.ball_dy = 2;
    pong_state.left_score = 0;
    pong_state.right_score = 0;
    pong_state.game_over = 0;
}

static void pong_render(window_t* win, int cx, int cy, int cw, int ch) {
    (void)win;
    gfx_fill_rect(cx, cy, cw, ch, 0xFF11111B);

    // 1. Header with Scoreboard
    gfx_fill_rect(cx, cy, cw, 28, 0xFF181825);
    gfx_fill_rect(cx, cy + 27, cw, 1, COLOR_OVERLAY);

    char score_buf[32];
    score_buf[0] = 'P'; score_buf[1] = 'l'; score_buf[2] = 'a'; score_buf[3] = 'y'; score_buf[4] = 'e'; score_buf[5] = 'r'; score_buf[6] = ':'; score_buf[7] = ' ';
    score_buf[8] = '0' + pong_state.left_score;
    score_buf[9] = ' '; score_buf[10] = '-'; score_buf[11] = ' ';
    score_buf[12] = '0' + pong_state.right_score;
    score_buf[13] = ' '; score_buf[14] = ':'; score_buf[15] = 'C'; score_buf[16] = 'P'; score_buf[17] = 'U'; score_buf[18] = '\0';

    int sw = font_get_string_width(score_buf);
    gfx_draw_text(cx + (cw - sw) / 2, cy + 6, score_buf, COLOR_LEMON_GOLD);
    gfx_draw_text(cx + 12, cy + 6, "First to 5 Wins", COLOR_TEXT_MUTED);

    // 2. Court Field
    int play_y = cy + 28;
    int play_h = ch - 28;

    // Center dotted line
    int net_x = cx + cw / 2;
    for (int y = play_y + 4; y < cy + ch - 4; y += 12) {
        gfx_fill_rect(net_x - 1, y, 2, 6, COLOR_OVERLAY);
    }

    // 3. Physics & AI
    if (!pong_state.game_over) {
        pong_state.ball_x += pong_state.ball_dx;
        pong_state.ball_y += pong_state.ball_dy;

        // Top & bottom wall bounce
        if (pong_state.ball_y <= play_y + 4) {
            pong_state.ball_y = play_y + 4;
            pong_state.ball_dy = -pong_state.ball_dy;
        } else if (pong_state.ball_y >= cy + ch - 12) {
            pong_state.ball_y = cy + ch - 12;
            pong_state.ball_dy = -pong_state.ball_dy;
        }

        // Left paddle hit
        int lp_x = cx + 18;
        int lp_y = play_y + pong_state.left_paddle_y;
        if (pong_state.ball_x <= lp_x + pong_state.paddle_w && pong_state.ball_x >= lp_x - 4 &&
            pong_state.ball_y >= lp_y - 4 && pong_state.ball_y <= lp_y + pong_state.paddle_h + 4) {
            pong_state.ball_dx = 3;
            int offset = (pong_state.ball_y - lp_y) - pong_state.paddle_h / 2;
            pong_state.ball_dy = offset / 8;
            if (pong_state.ball_dy == 0) pong_state.ball_dy = (offset < 0) ? -2 : 2;
        }

        // Right paddle AI tracker
        int rp_center = play_y + pong_state.right_paddle_y + pong_state.paddle_h / 2;
        if (rp_center < pong_state.ball_y - 4) {
            pong_state.right_paddle_y += 2;
        } else if (rp_center > pong_state.ball_y + 4) {
            pong_state.right_paddle_y -= 2;
        }
        if (pong_state.right_paddle_y < 4) pong_state.right_paddle_y = 4;
        if (pong_state.right_paddle_y > play_h - pong_state.paddle_h - 4) pong_state.right_paddle_y = play_h - pong_state.paddle_h - 4;

        // Right paddle hit
        int rp_x = cx + cw - 18 - pong_state.paddle_w;
        int rp_y = play_y + pong_state.right_paddle_y;
        if (pong_state.ball_x >= rp_x - 4 && pong_state.ball_x <= rp_x + pong_state.paddle_w &&
            pong_state.ball_y >= rp_y - 4 && pong_state.ball_y <= rp_y + pong_state.paddle_h + 4) {
            pong_state.ball_dx = -3;
            int offset = (pong_state.ball_y - rp_y) - pong_state.paddle_h / 2;
            pong_state.ball_dy = offset / 8;
            if (pong_state.ball_dy == 0) pong_state.ball_dy = (offset < 0) ? -2 : 2;
        }

        // Left goal (CPU scores)
        if (pong_state.ball_x < cx) {
            pong_state.right_score++;
            pong_state.ball_x = cx + cw / 2;
            pong_state.ball_y = play_y + play_h / 2;
            pong_state.ball_dx = 3;
            if (pong_state.right_score >= 5) pong_state.game_over = 1;
        }

        // Right goal (Player scores)
        if (pong_state.ball_x > cx + cw) {
            pong_state.left_score++;
            pong_state.ball_x = cx + cw / 2;
            pong_state.ball_y = play_y + play_h / 2;
            pong_state.ball_dx = -3;
            if (pong_state.left_score >= 5) pong_state.game_over = 1;
        }
    }

    // 4. Render Paddles
    int lp_x = cx + 18;
    int lp_y = play_y + pong_state.left_paddle_y;
    gfx_fill_rounded_rect(lp_x, lp_y, pong_state.paddle_w, pong_state.paddle_h, 4, COLOR_BLUE_ACCENT);

    int rp_x = cx + cw - 18 - pong_state.paddle_w;
    int rp_y = play_y + pong_state.right_paddle_y;
    gfx_fill_rounded_rect(rp_x, rp_y, pong_state.paddle_w, pong_state.paddle_h, 4, COLOR_PURPLE_ACCENT);

    // 5. Render Ball
    gfx_fill_rounded_rect(pong_state.ball_x - 4, pong_state.ball_y - 4, 8, 8, 4, 0xFFFFFFFF);

    // 6. Game Over Overlay
    if (pong_state.game_over) {
        int ov_w = 220;
        int ov_h = 70;
        int ov_x = cx + (cw - ov_w) / 2;
        int ov_y = cy + (ch - ov_h) / 2;
        gfx_fill_rounded_rect(ov_x, ov_y, ov_w, ov_h, 8, 0xFF11111B);
        gfx_draw_rounded_rect(ov_x, ov_y, ov_w, ov_h, 8, COLOR_BORDER);

        const char* msg = (pong_state.left_score > pong_state.right_score) ? "YOU WON THE MATCH!" : "CPU WINS!";
        uint32_t col = (pong_state.left_score > pong_state.right_score) ? COLOR_GREEN_ACCENT : COLOR_RED_ACCENT;
        int mw = font_get_string_width(msg);
        gfx_draw_text(ov_x + (ov_w - mw) / 2, ov_y + 14, msg, col);
        int rw = font_get_string_width("Click to Play Again");
        gfx_draw_text(ov_x + (ov_w - rw) / 2, ov_y + 38, "Click to Play Again", COLOR_TEXT_PRIMARY);
    }
}

static void pong_on_mouse(window_t* win, int rx, int ry, int buttons, int event_type) {
    (void)rx; (void)buttons;
    if (event_type == WM_EVENT_MOUSE_CLICK && pong_state.game_over) {
        pong_reset();
        return;
    }

    // Move left paddle directly to mouse Y
    int play_h = win->height - 35 - 28;
    int target_y = ry - 28 - pong_state.paddle_h / 2;
    if (target_y < 4) target_y = 4;
    if (target_y > play_h - pong_state.paddle_h - 4) target_y = play_h - pong_state.paddle_h - 4;
    pong_state.left_paddle_y = target_y;
}

static void pong_on_key(window_t* win, char key) {
    (void)win;
    if (pong_state.game_over) {
        if (key == ' ' || key == '\n') pong_reset();
        return;
    }

    int play_h = win->height - 35 - 28;
    if (key == 'w' || key == 'W') {
        pong_state.left_paddle_y -= 12;
        if (pong_state.left_paddle_y < 4) pong_state.left_paddle_y = 4;
    } else if (key == 's' || key == 'S') {
        pong_state.left_paddle_y += 12;
        if (pong_state.left_paddle_y > play_h - pong_state.paddle_h - 4) {
            pong_state.left_paddle_y = play_h - pong_state.paddle_h - 4;
        }
    }
}

static void pong_on_close(window_t* win) {
    (void)win;
    pong_win = NULL;
}

void gui_pong_open(void) {
    if (pong_win && !pong_win->is_closed) {
        if (pong_win->is_minimized) wm_restore_window(pong_win);
        wm_bring_to_front(pong_win);
        return;
    }

    pong_reset();
    pong_win = wm_create_window("Pong Arcade", ICON_GAMES, 170, 95, 460, 340, pong_render);
    if (pong_win) {
        pong_win->on_mouse = pong_on_mouse;
        pong_win->on_key = pong_on_key;
        pong_win->on_close = pong_on_close;
    }
}
