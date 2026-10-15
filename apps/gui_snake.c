#include "gui_snake.h"
#include "graphics.h"
#include "font.h"
#include <stdint.h>
#include <stddef.h>

#define GRID_W 20
#define GRID_H 18
#define CELL_SZ 16
#define MAX_SNAKE 100

typedef struct {
    int snake_x[MAX_SNAKE];
    int snake_y[MAX_SNAKE];
    int length;
    int dir; // 0=up, 1=right, 2=down, 3=left
    int next_dir;
    int food_x, food_y;
    int score;
    int high_score;
    int game_over;
    int tick_counter;
} snake_state_t;

static snake_state_t snake_state;
static window_t* snake_win = NULL;

static void snake_spawn_food(void) {
    // Pseudorandom spawn
    static uint32_t seed = 42;
    seed = seed * 1103515245 + 12345;
    snake_state.food_x = (seed >> 16) % GRID_W;
    seed = seed * 1103515245 + 12345;
    snake_state.food_y = (seed >> 16) % GRID_H;

    for (int i = 0; i < snake_state.length; i++) {
        if (snake_state.snake_x[i] == snake_state.food_x && snake_state.snake_y[i] == snake_state.food_y) {
            snake_state.food_x = (snake_state.food_x + 3) % GRID_W;
            snake_state.food_y = (snake_state.food_y + 2) % GRID_H;
            break;
        }
    }
}

static void snake_reset(void) {
    snake_state.length = 4;
    snake_state.snake_x[0] = 6; snake_state.snake_y[0] = 8;
    snake_state.snake_x[1] = 5; snake_state.snake_y[1] = 8;
    snake_state.snake_x[2] = 4; snake_state.snake_y[2] = 8;
    snake_state.snake_x[3] = 3; snake_state.snake_y[3] = 8;

    snake_state.dir = 1;
    snake_state.next_dir = 1;
    snake_state.score = 0;
    snake_state.game_over = 0;
    snake_state.tick_counter = 0;
    snake_spawn_food();
}

static void snake_render(window_t* win, int cx, int cy, int cw, int ch) {
    (void)win; (void)cw; (void)ch;
    // Dark game background
    gfx_fill_rect(cx, cy, cw, ch, 0xFF11111B);

    // 1. Header Bar: Score & Controls Info
    gfx_fill_rect(cx, cy, cw, 28, 0xFF181825);
    gfx_fill_rect(cx, cy + 27, cw, 1, COLOR_OVERLAY);

    char score_buf[24];
    score_buf[0] = 'S'; score_buf[1] = 'c'; score_buf[2] = 'o'; score_buf[3] = 'r'; score_buf[4] = 'e'; score_buf[5] = ':'; score_buf[6] = ' ';
    int sp = 7;
    int sc = snake_state.score;
    if (sc == 0) {
        score_buf[sp++] = '0';
    } else {
        char temp[16]; int tp = 0;
        while (sc > 0) { temp[tp++] = '0' + (sc % 10); sc /= 10; }
        while (tp > 0) score_buf[sp++] = temp[--tp];
    }
    score_buf[sp] = '\0';
    gfx_draw_text(cx + 12, cy + 6, score_buf, COLOR_LEMON_GOLD);
    gfx_draw_text(cx + cw - 160, cy + 6, "WASD / Arrows to Move", COLOR_TEXT_MUTED);

    // 2. Game Grid Area
    int grid_x = cx + (cw - GRID_W * CELL_SZ) / 2;
    int grid_y = cy + 34;

    // Grid border
    gfx_draw_rounded_rect(grid_x - 2, grid_y - 2, GRID_W * CELL_SZ + 4, GRID_H * CELL_SZ + 4, 4, COLOR_BORDER);
    gfx_fill_rect(grid_x, grid_y, GRID_W * CELL_SZ, GRID_H * CELL_SZ, 0xFF181825);

    // Update game tick (~6 moves/sec)
    if (!snake_state.game_over) {
        snake_state.tick_counter++;
        if (snake_state.tick_counter >= 5) {
            snake_state.tick_counter = 0;
            snake_state.dir = snake_state.next_dir;

            // Move body
            for (int i = snake_state.length - 1; i > 0; i--) {
                snake_state.snake_x[i] = snake_state.snake_x[i - 1];
                snake_state.snake_y[i] = snake_state.snake_y[i - 1];
            }

            // Move head
            if (snake_state.dir == 0) snake_state.snake_y[0]--;
            else if (snake_state.dir == 1) snake_state.snake_x[0]++;
            else if (snake_state.dir == 2) snake_state.snake_y[0]++;
            else if (snake_state.dir == 3) snake_state.snake_x[0]--;

            // Wall collisions
            if (snake_state.snake_x[0] < 0 || snake_state.snake_x[0] >= GRID_W ||
                snake_state.snake_y[0] < 0 || snake_state.snake_y[0] >= GRID_H) {
                snake_state.game_over = 1;
            }

            // Self collisions
            for (int i = 1; i < snake_state.length; i++) {
                if (snake_state.snake_x[0] == snake_state.snake_x[i] &&
                    snake_state.snake_y[0] == snake_state.snake_y[i]) {
                    snake_state.game_over = 1;
                    break;
                }
            }

            // Food collision
            if (snake_state.snake_x[0] == snake_state.food_x && snake_state.snake_y[0] == snake_state.food_y) {
                if (snake_state.length < MAX_SNAKE - 1) {
                    snake_state.length++;
                }
                snake_state.score += 10;
                if (snake_state.score > snake_state.high_score) {
                    snake_state.high_score = snake_state.score;
                }
                snake_spawn_food();
            }
        }
    }

    // 3. Draw Food (Red Apple with green leaf)
    int fx = grid_x + snake_state.food_x * CELL_SZ;
    int fy = grid_y + snake_state.food_y * CELL_SZ;
    gfx_fill_rounded_rect(fx + 2, fy + 3, CELL_SZ - 4, CELL_SZ - 5, 4, COLOR_RED_ACCENT);
    gfx_put_pixel(fx + CELL_SZ / 2, fy + 1, COLOR_GREEN_ACCENT);

    // 4. Draw Snake
    for (int i = 0; i < snake_state.length; i++) {
        int sx = grid_x + snake_state.snake_x[i] * CELL_SZ;
        int sy = grid_y + snake_state.snake_y[i] * CELL_SZ;

        if (i == 0) {
            // Head
            gfx_fill_rounded_rect(sx + 1, sy + 1, CELL_SZ - 2, CELL_SZ - 2, 4, COLOR_GREEN_ACCENT);
            // Eyes
            gfx_put_pixel(sx + 4, sy + 4, 0xFF11111B);
            gfx_put_pixel(sx + CELL_SZ - 5, sy + 4, 0xFF11111B);
        } else {
            // Body segments
            uint32_t col = (i % 2 == 0) ? 0xFF85D480 : 0xFF71BD6C;
            gfx_fill_rounded_rect(sx + 2, sy + 2, CELL_SZ - 4, CELL_SZ - 4, 3, col);
        }
    }

    // 5. Game Over Overlay
    if (snake_state.game_over) {
        int ov_w = 200;
        int ov_h = 70;
        int ov_x = grid_x + (GRID_W * CELL_SZ - ov_w) / 2;
        int ov_y = grid_y + (GRID_H * CELL_SZ - ov_h) / 2;
        gfx_fill_rounded_rect(ov_x, ov_y, ov_w, ov_h, 8, 0xFF11111B);
        gfx_draw_rounded_rect(ov_x, ov_y, ov_w, ov_h, 8, COLOR_RED_ACCENT);

        int gw = font_get_string_width("GAME OVER");
        gfx_draw_text(ov_x + (ov_w - gw) / 2, ov_y + 14, "GAME OVER", COLOR_RED_ACCENT);
        int rw = font_get_string_width("Press Space or Click to Restart");
        gfx_draw_text(ov_x + (ov_w - rw) / 2, ov_y + 38, "Press Space to Restart", COLOR_TEXT_PRIMARY);
    }
}

static void snake_on_key(window_t* win, char key) {
    (void)win;
    if (snake_state.game_over) {
        if (key == ' ' || key == '\n') {
            snake_reset();
        }
        return;
    }

    if ((key == 'w' || key == 'W' || key == 0x11) && snake_state.dir != 2) {
        snake_state.next_dir = 0; // Up
    } else if ((key == 'd' || key == 'D' || key == 0x14) && snake_state.dir != 3) {
        snake_state.next_dir = 1; // Right
    } else if ((key == 's' || key == 'S' || key == 0x12) && snake_state.dir != 0) {
        snake_state.next_dir = 2; // Down
    } else if ((key == 'a' || key == 'A' || key == 0x13) && snake_state.dir != 1) {
        snake_state.next_dir = 3; // Left
    }
}

static void snake_on_mouse(window_t* win, int rx, int ry, int buttons, int event_type) {
    (void)win; (void)rx; (void)ry; (void)buttons;
    if (event_type == WM_EVENT_MOUSE_CLICK && snake_state.game_over) {
        snake_reset();
    }
}

static void snake_on_close(window_t* win) {
    (void)win;
    snake_win = NULL;
}

void gui_snake_open(void) {
    if (snake_win && !snake_win->is_closed) {
        if (snake_win->is_minimized) wm_restore_window(snake_win);
        wm_bring_to_front(snake_win);
        return;
    }

    snake_reset();
    snake_win = wm_create_window("Snake Game", ICON_GAMES, 150, 80, 360, 360, snake_render);
    if (snake_win) {
        snake_win->on_key = snake_on_key;
        snake_win->on_mouse = snake_on_mouse;
        snake_win->on_close = snake_on_close;
    }
}
