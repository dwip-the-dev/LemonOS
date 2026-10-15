#include "gui_matrix.h"
#include "graphics.h"
#include "font.h"
#include <stdint.h>
#include <stddef.h>

#define MATRIX_COLS 50

typedef struct {
    int drop_y[MATRIX_COLS];
    int drop_speed[MATRIX_COLS];
    char drop_char[MATRIX_COLS];
} matrix_state_t;

static matrix_state_t matrix_state;
static window_t* matrix_win = NULL;

static void matrix_init_drops(void) {
    static uint32_t seed = 12345;
    for (int i = 0; i < MATRIX_COLS; i++) {
        seed = seed * 1103515245 + 12345;
        matrix_state.drop_y[i] = -((int)(seed % 20));
        matrix_state.drop_speed[i] = 1 + (seed % 3);
        matrix_state.drop_char[i] = '0' + (seed % 10);
    }
}

static void matrix_render(window_t* win, int cx, int cy, int cw, int ch) {
    (void)win;
    // Pure pitch black background
    gfx_fill_rect(cx, cy, cw, ch, 0xFF050508);

    int num_cols = cw / FONT_WIDTH;
    if (num_cols > MATRIX_COLS) num_cols = MATRIX_COLS;

    static uint32_t seed = 777;

    for (int c = 0; c < num_cols; c++) {
        matrix_state.drop_y[c] += matrix_state.drop_speed[c];

        seed = seed * 1103515245 + 12345;
        // Randomly morph character
        if ((seed % 5) == 0) {
            matrix_state.drop_char[c] = 33 + (seed % 90);
        }

        int head_y = cy + matrix_state.drop_y[c] * (FONT_HEIGHT - 2);

        // Draw glowing trail behind head
        for (int t = 0; t < 12; t++) {
            int ty = head_y - t * (FONT_HEIGHT - 2);
            if (ty >= cy && ty < cy + ch - 16) {
                int tx = cx + c * FONT_WIDTH;
                char tc = (char)(matrix_state.drop_char[c] + t);
                if (tc < 33 || tc > 126) tc = '0' + (t % 10);

                uint32_t color;
                if (t == 0) color = 0xFFFFFFFF; // Glowing bright white head
                else if (t < 3) color = 0xFFA6E3A1; // Light phosphor green
                else if (t < 7) color = 0xFF40C057; // Medium digital green
                else color = 0xFF1B5E20; // Dim trailing green

                font_draw_char(gfx_get_backbuffer(), SCREEN_WIDTH, SCREEN_HEIGHT, tx, ty, tc, color, 0, 1);
            }
        }

        // Reset drop when off screen
        if (head_y > cy + ch + 150) {
            seed = seed * 1103515245 + 12345;
            matrix_state.drop_y[c] = -((int)(seed % 10));
            matrix_state.drop_speed[c] = 1 + (seed % 3);
        }
    }
}

static void matrix_on_close(window_t* win) {
    (void)win;
    matrix_win = NULL;
}

void gui_matrix_open(void) {
    if (matrix_win && !matrix_win->is_closed) {
        if (matrix_win->is_minimized) wm_restore_window(matrix_win);
        wm_bring_to_front(matrix_win);
        return;
    }

    matrix_init_drops();
    matrix_win = wm_create_window("Matrix Digital Rain", ICON_TERMINAL, 190, 80, 480, 360, matrix_render);
    if (matrix_win) {
        matrix_win->on_close = matrix_on_close;
    }
}
