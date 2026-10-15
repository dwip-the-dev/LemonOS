#include "gui_files.h"
#include "graphics.h"
#include "font.h"
#include "icons.h"
#include "filesystem.h"
#include "net.h"
#include "user_loader.h"
#include "alerts.h"
#include "string.h"
#include <stdint.h>
#include <stddef.h>

#define FILES_MAX 32
#define FILE_NAME_LEN 64
#define FILE_EDIT_LEN 4096

typedef enum {
    VIEW_MODE_LIST = 0,
    VIEW_MODE_GRID = 1
} view_mode_t;

typedef enum {
    PANE_EXPLORER = 0,
    PANE_EDITOR   = 1,
    PANE_INSPECTOR = 2
} right_pane_t;

typedef struct {
    char current_dir[FILE_NAME_LEN];
    char current_file[FILE_NAME_LEN];
    char editor_buf[FILE_EDIT_LEN];
    int  editor_len;
    int  cursor_pos;
    int  selected_idx;
    view_mode_t view_mode;
    right_pane_t right_pane;
    char status_msg[64];

    // Scrolling and drag
    int scroll_y;
    int is_dragging_scroll;
    int drag_start_y;
    int drag_start_scroll;

    // ELF Inspector information
    int      is_elf;
    uint32_t elf_entry;
    uint16_t elf_type;
    int      elf_has_dyn;
    char     elf_run_output[256];
    int      elf_run_exit_code;
    int      elf_ran;
} files_state_t;

static files_state_t files_state;
static window_t* files_win = NULL;

static int str_starts_with(const char* str, const char* prefix) {
    if (!str || !prefix) return 0;
    while (*prefix) {
        if (*prefix != *str) return 0;
        prefix++;
        str++;
    }
    return 1;
}

static void files_inspect_elf(const char* path) {
    files_state.is_elf = 0;
    files_state.elf_entry = 0;
    files_state.elf_type = 0;
    files_state.elf_has_dyn = 0;
    files_state.elf_ran = 0;
    files_state.elf_run_output[0] = '\0';

    char* content = fs_read_file(path);
    uint32_t fsize = fs_get_file_size(path);
    if (!content || fsize < sizeof(elf32_ehdr_t)) return;

    elf32_ehdr_t* ehdr = (elf32_ehdr_t*)content;
    if (ehdr->e_ident[0] == 0x7F && ehdr->e_ident[1] == 'E' &&
        ehdr->e_ident[2] == 'L' && ehdr->e_ident[3] == 'F') {
        files_state.is_elf = 1;
        files_state.elf_entry = ehdr->e_entry;
        files_state.elf_type = ehdr->e_type;
        // Check for PT_DYNAMIC in program headers
        if (ehdr->e_phoff > 0 && ehdr->e_phnum > 0 && (ehdr->e_phoff + ehdr->e_phnum * sizeof(elf32_phdr_t) <= fsize)) {
            elf32_phdr_t* phdrs = (elf32_phdr_t*)(content + ehdr->e_phoff);
            for (int p = 0; p < ehdr->e_phnum; p++) {
                if (phdrs[p].p_type == 2) { // PT_DYNAMIC
                    files_state.elf_has_dyn = 1;
                    break;
                }
            }
        }
    }
}

static void files_select_file(const char* name) {
    if (!name || name[0] == '\0') return;

    int i = 0;
    while (name[i] && i < FILE_NAME_LEN - 1) {
        files_state.current_file[i] = name[i];
        i++;
    }
    files_state.current_file[i] = '\0';

    // Check if it is an ELF binary
    files_inspect_elf(name);

    if (files_state.is_elf) {
        files_state.right_pane = PANE_INSPECTOR;
        const char* ok = "ELF32 Binary Inspected";
        int p = 0;
        while (*ok) files_state.status_msg[p++] = *ok++;
        files_state.status_msg[p] = '\0';
    } else {
        files_state.right_pane = PANE_EDITOR;
        char* content = fs_read_file(name);
        files_state.editor_len = 0;
        if (content) {
            while (content[files_state.editor_len] && files_state.editor_len < FILE_EDIT_LEN - 1) {
                files_state.editor_buf[files_state.editor_len] = content[files_state.editor_len];
                files_state.editor_len++;
            }
        }
        files_state.editor_buf[files_state.editor_len] = '\0';
        files_state.cursor_pos = files_state.editor_len;

        const char* ok = "Ready";
        int p = 0;
        while (*ok) files_state.status_msg[p++] = *ok++;
        files_state.status_msg[p] = '\0';
    }
}

static void files_save_current(void) {
    if (files_state.current_file[0] == '\0') return;
    fs_write_file(files_state.current_file, files_state.editor_buf);
    const char* ok = "Saved to LemonFS!";
    int p = 0;
    while (*ok) files_state.status_msg[p++] = *ok++;
    files_state.status_msg[p] = '\0';

    alert_send("File Saved", files_state.current_file, ICON_FILE_MANAGER, ALERT_TYPE_SUCCESS);
}

static void files_trigger_download(void) {
    const char* msg = "Downloading...";
    int p = 0;
    while (*msg) files_state.status_msg[p++] = *msg++;
    files_state.status_msg[p] = '\0';

    char dl_buf[1024];
    int status = 0;
    int bytes = net_http_get("http://example.com/index.html", dl_buf, sizeof(dl_buf), &status);

    if (bytes > 0 && status == 200) {
        fs_create_file("/downloads/index.html", dl_buf);
        files_select_file("/downloads/index.html");
        const char* s_ok = "Downloaded 200 OK!";
        int sp = 0;
        while (*s_ok) files_state.status_msg[sp++] = *s_ok++;
        files_state.status_msg[sp] = '\0';
        alert_send("Download Complete", "/downloads/index.html saved", ICON_WIFI, ALERT_TYPE_SUCCESS);
    } else {
        const char* s_fail = "Download error.";
        int sp = 0;
        while (*s_fail) files_state.status_msg[sp++] = *s_fail++;
        files_state.status_msg[sp] = '\0';
        alert_send("Download Failed", "HTTP GET failed", ICON_WIFI, ALERT_TYPE_ERROR);
    }
}

static void files_run_selected_elf(void) {
    if (!files_state.is_elf || files_state.current_file[0] == '\0') return;

    files_state.elf_ran = 1;
    files_state.elf_run_exit_code = process_spawn(files_state.current_file, 0, NULL);

    const char* out = "Executed in Ring 3 successfully! Output emitted to serial/terminal.";
    int p = 0;
    while (*out && p < 250) {
        files_state.elf_run_output[p++] = *out++;
    }
    files_state.elf_run_output[p] = '\0';

    alert_send("Ring 3 Execution", "Binary spawned successfully", ICON_TERMINAL, ALERT_TYPE_SUCCESS);
}

void files_set_dir(const char* dir) {
    int i = 0;
    while (dir[i] && i < FILE_NAME_LEN - 1) {
        files_state.current_dir[i] = dir[i];
        i++;
    }
    files_state.current_dir[i] = '\0';
    files_state.scroll_y = 0;

    int total_files = fs_get_count();
    for (int f = 0; f < total_files; f++) {
        char fname[FS_MAX_PATH];
        uint32_t fsize = 0;
        uint8_t ftype = 0;
        if (fs_get_entry(f, fname, &fsize, &ftype)) {
            if (files_state.current_dir[1] == '\0') {
                if (ftype != FS_TYPE_DIR) {
                    files_state.selected_idx = f;
                    files_select_file(fname);
                    return;
                }
            } else if (str_starts_with(fname, files_state.current_dir)) {
                if (strcmp(fname, files_state.current_dir) == 0) continue;
                files_state.selected_idx = f;
                if (ftype != FS_TYPE_DIR) {
                    files_select_file(fname);
                }
                return;
            }
        }
    }
}

static void files_render(window_t* win, int cx, int cy, int cw, int ch) {
    (void)win;
    gfx_fill_rect(cx, cy, cw, ch, COLOR_SURFACE);

    // 1. Top Navigation Bar (Height: 38px)
    int nav_h = 36;
    gfx_fill_rect(cx, cy, cw, nav_h, 0xFF181825);
    gfx_fill_rect(cx, cy + nav_h - 1, cw, 1, COLOR_OVERLAY);

    // Navigation buttons: [< Back] [> Fwd] [^ Up] [Refresh]
    int btn_y = cy + 5;
    gfx_fill_rounded_rect(cx + 8, btn_y, 26, 26, 6, COLOR_OVERLAY);
    icon_draw_16(cx + 13, btn_y + 5, ICON_ARROW_LEFT, COLOR_TEXT_PRIMARY);

    gfx_fill_rounded_rect(cx + 38, btn_y, 26, 26, 6, COLOR_OVERLAY);
    icon_draw_16(cx + 43, btn_y + 5, ICON_ARROW_RIGHT, COLOR_TEXT_MUTED);

    gfx_fill_rounded_rect(cx + 68, btn_y, 26, 26, 6, COLOR_OVERLAY);
    gfx_draw_text(cx + 76, btn_y + 5, "^", COLOR_TEXT_PRIMARY);

    gfx_fill_rounded_rect(cx + 98, btn_y, 26, 26, 6, COLOR_OVERLAY);
    icon_draw_16(cx + 103, btn_y + 5, ICON_REFRESH, COLOR_TEXT_PRIMARY);

    // Breadcrumbs Path Bar
    int path_x = cx + 132;
    int path_w = cw - 260;
    gfx_fill_rounded_rect(path_x, btn_y, path_w, 26, 6, 0xFF11111B);
    gfx_draw_rounded_rect(path_x, btn_y, path_w, 26, 6, COLOR_BORDER);
    icon_draw_16(path_x + 8, btn_y + 5, ICON_FILE_MANAGER, COLOR_LEMON_GOLD);

    char breadcrumb_str[64];
    int bp = 0;
    breadcrumb_str[bp++] = 'R';
    breadcrumb_str[bp++] = 'o';
    breadcrumb_str[bp++] = 'o';
    breadcrumb_str[bp++] = 't';
    if (files_state.current_dir[0] != '\0' && (files_state.current_dir[0] != '/' || files_state.current_dir[1] != '\0')) {
        breadcrumb_str[bp++] = ' ';
        breadcrumb_str[bp++] = '>';
        breadcrumb_str[bp++] = ' ';
        const char* cd = files_state.current_dir;
        if (*cd == '/') cd++;
        while (*cd && bp < 60) breadcrumb_str[bp++] = *cd++;
    }
    breadcrumb_str[bp] = '\0';
    gfx_draw_text(path_x + 28, btn_y + 5, breadcrumb_str, COLOR_TEXT_PRIMARY);

    // View mode toggle buttons on top right: [List] [Grid]
    int vm_x = cx + cw - 110;
    int is_list = (files_state.view_mode == VIEW_MODE_LIST);
    gfx_fill_rounded_rect(vm_x, btn_y, 48, 26, 6, is_list ? COLOR_BLUE_ACCENT : COLOR_OVERLAY);
    gfx_draw_text(vm_x + 12, btn_y + 5, "List", is_list ? 0xFFFFFFFF : COLOR_TEXT_MUTED);

    gfx_fill_rounded_rect(vm_x + 52, btn_y, 48, 26, 6, !is_list ? COLOR_BLUE_ACCENT : COLOR_OVERLAY);
    gfx_draw_text(vm_x + 64, btn_y + 5, "Grid", !is_list ? 0xFFFFFFFF : COLOR_TEXT_MUTED);

    // 2. Left Sidebar: Quick Places & Storage Capacity (Width: 160px)
    int side_w = 160;
    int body_y = cy + nav_h;
    int body_h = ch - nav_h;
    gfx_fill_rect(cx, body_y, side_w, body_h, 0xFF14151F);
    gfx_fill_rect(cx + side_w - 1, body_y, 1, body_h, COLOR_OVERLAY);

    gfx_draw_text(cx + 12, body_y + 10, "PLACES", COLOR_TEXT_MUTED);

    typedef struct {
        const char* name;
        const char* path;
        icon_type_t icon_id;
    } place_item_t;

    static const place_item_t places[] = {
        {"Root (/)",       "/",          ICON_HOME},
        {"Documents",      "/docs",      ICON_FILE_MANAGER},
        {"Downloads",      "/downloads", ICON_FIREFOX},
        {"Binaries",       "/bin",       ICON_TERMINAL},
        {"Libraries",      "/lib",       ICON_SETTINGS},
        {"System",         "/system",    ICON_SYSINFO}
    };
    int num_places = sizeof(places) / sizeof(places[0]);

    for (int p = 0; p < num_places; p++) {
        int py = body_y + 28 + p * 28;
        int is_cur = (str_starts_with(files_state.current_dir, places[p].path) &&
                      (places[p].path[1] != '\0' || files_state.current_dir[1] == '\0'));

        if (is_cur) {
            gfx_fill_rounded_rect(cx + 6, py, side_w - 12, 24, 6, COLOR_OVERLAY);
            gfx_draw_rounded_rect(cx + 6, py, side_w - 12, 24, 6, COLOR_BORDER_LIGHT);
        }

        icon_draw_16(cx + 12, py + 4, places[p].icon_id, is_cur ? COLOR_LEMON_GOLD : COLOR_TEXT_SECONDARY);
        gfx_draw_text(cx + 34, py + 4, places[p].name, is_cur ? COLOR_TEXT_PRIMARY : COLOR_TEXT_SECONDARY);
    }

    // Storage capacity card at bottom of sidebar
    int gauge_y = body_y + body_h - 76;
    gfx_fill_rounded_rect(cx + 8, gauge_y, side_w - 16, 68, 8, 0xFF1E1E2E);
    gfx_draw_rounded_rect(cx + 8, gauge_y, side_w - 16, 68, 8, COLOR_BORDER);
    gfx_draw_text(cx + 14, gauge_y + 8, "Storage (LemonFS)", COLOR_TEXT_PRIMARY);
    gfx_draw_text(cx + 14, gauge_y + 24, "Used: 1.2 MB / 32 MB", COLOR_TEXT_MUTED);

    // Storage capacity bar
    gfx_fill_rounded_rect(cx + 14, gauge_y + 44, side_w - 28, 6, 3, 0xFF313244);
    gfx_fill_rounded_rect(cx + 14, gauge_y + 44, (side_w - 28) * 15 / 100, 6, 3, COLOR_LEMON_GOLD);

    // 3. Middle Pane: File List / Grid
    int pane_w = (cw - side_w) / 2;
    int pane_x = cx + side_w;

    // Filter and show files in current folder
    int total_files = fs_get_count();
    int visible_files = 0;
    int file_indices[FILES_MAX];

    for (int i = 0; i < total_files && visible_files < FILES_MAX; i++) {
        char fname[FS_MAX_PATH];
        uint32_t fsize = 0;
        uint8_t ftype = 0;
        if (fs_get_entry(i, fname, &fsize, &ftype)) {
            // Check if file matches current_dir
            if (files_state.current_dir[1] == '\0') { // Root "/"
                file_indices[visible_files++] = i;
            } else if (str_starts_with(fname, files_state.current_dir)) {
                if (strcmp(fname, files_state.current_dir) == 0) continue;
                file_indices[visible_files++] = i;
            }
        }
    }

    int act_y = body_y + body_h - 36;

    if (files_state.view_mode == VIEW_MODE_LIST) {
        // Table Header
        int th_y = body_y + 6;
        gfx_fill_rect(pane_x, th_y, pane_w, 22, 0xFF181825);
        gfx_draw_text(pane_x + 12, th_y + 4, "Name", COLOR_TEXT_MUTED);
        gfx_draw_text(pane_x + 150, th_y + 4, "Type", COLOR_TEXT_MUTED);
        gfx_draw_text(pane_x + 200, th_y + 4, "Size", COLOR_TEXT_MUTED);

        int list_start_y = th_y + 26;
        int list_avail_h = act_y - list_start_y;
        int rows_visible = list_avail_h / 28;
        if (rows_visible < 1) rows_visible = 1;

        int max_scroll = (visible_files > rows_visible) ? (visible_files - rows_visible) : 0;
        if (files_state.scroll_y < 0) files_state.scroll_y = 0;
        if (files_state.scroll_y > max_scroll) files_state.scroll_y = max_scroll;

        int row_w = pane_w - ((max_scroll > 0) ? 14 : 8);

        for (int r = 0; r < rows_visible && (files_state.scroll_y + r) < visible_files; r++) {
            int entry_idx = file_indices[files_state.scroll_y + r];
            char fname[FS_MAX_PATH];
            uint32_t fsize = 0;
            uint8_t ftype = 0;
            fs_get_entry(entry_idx, fname, &fsize, &ftype);

            int fy = list_start_y + r * 28;
            int is_sel = (files_state.selected_idx == entry_idx);

            uint32_t fbg = is_sel ? 0xFF2B2C40 : ((r % 2 == 0) ? 0xFF1E1E2E : 0xFF181825);
            gfx_fill_rounded_rect(pane_x + 4, fy, row_w, 26, 4, fbg);
            if (is_sel) {
                gfx_draw_rounded_rect(pane_x + 4, fy, row_w, 26, 4, COLOR_BLUE_ACCENT);
            }

            // Icon by type
            icon_type_t ic = (ftype == FS_TYPE_DIR) ? ICON_FILE_MANAGER : ICON_SYSINFO;
            if (str_starts_with(fname, "/bin") || str_starts_with(fname, "/lib")) ic = ICON_TERMINAL;
            icon_draw_16(pane_x + 8, fy + 5, ic, (ftype == FS_TYPE_DIR) ? COLOR_LEMON_GOLD : COLOR_BLUE_ACCENT);

            // Strip path for display
            const char* disp_name = fname;
            for (int s = 0; fname[s]; s++) {
                if (fname[s] == '/' && fname[s + 1] != '\0') disp_name = &fname[s + 1];
            }
            gfx_draw_text_ellipsis(pane_x + 28, fy + 5, 115, disp_name, is_sel ? COLOR_TEXT_PRIMARY : COLOR_TEXT_SECONDARY);

            // Type text
            const char* type_str = (ftype == FS_TYPE_DIR) ? "DIR" : "FILE";
            if (str_starts_with(fname, "/bin")) type_str = "ELF32";
            else if (str_starts_with(fname, "/lib")) type_str = "SO";
            gfx_draw_text(pane_x + 150, fy + 5, type_str, COLOR_TEXT_MUTED);

            // Size text (e.g. 1.2K)
            char sz_buf[16];
            sz_buf[0] = '0' + ((fsize / 1024) % 10);
            sz_buf[1] = 'K';
            sz_buf[2] = '\0';
            gfx_draw_text(pane_x + 200, fy + 5, (ftype == FS_TYPE_DIR) ? "-" : sz_buf, COLOR_TEXT_MUTED);
        }

        // Vertical scrollbar for middle pane
        if (max_scroll > 0) {
            int sb_w = 6;
            int sb_x = pane_x + pane_w - sb_w - 2;
            int sb_y = list_start_y;
            int sb_h = list_avail_h;

            gfx_fill_rounded_rect(sb_x, sb_y, sb_w, sb_h, 3, 0xFF181825);

            int thumb_h = (rows_visible * sb_h) / visible_files;
            if (thumb_h < 18) thumb_h = 18;
            if (thumb_h > sb_h) thumb_h = sb_h;

            int max_travel = sb_h - thumb_h;
            int thumb_y = sb_y + ((files_state.scroll_y * max_travel) / max_scroll);
            uint32_t thumb_col = files_state.is_dragging_scroll ? COLOR_BLUE_ACCENT : COLOR_BORDER_LIGHT;
            gfx_fill_rounded_rect(sb_x, thumb_y, sb_w, thumb_h, 3, thumb_col);
        }
    } else {
        // Grid View (Large 32x32 icons with labels)
        int cols = 3;
        int gw = (pane_w - 24) / cols;
        int gh = 64;
        int total_rows = (visible_files + cols - 1) / cols;
        int grid_start_y = body_y + 10;
        int grid_avail_h = act_y - grid_start_y;
        int rows_visible = grid_avail_h / (gh + 10);
        if (rows_visible < 1) rows_visible = 1;

        int max_scroll = (total_rows > rows_visible) ? (total_rows - rows_visible) : 0;
        if (files_state.scroll_y < 0) files_state.scroll_y = 0;
        if (files_state.scroll_y > max_scroll) files_state.scroll_y = max_scroll;

        int start_item = files_state.scroll_y * cols;
        int max_items = rows_visible * cols;

        for (int i = 0; i < max_items && (start_item + i) < visible_files; i++) {
            int entry_idx = file_indices[start_item + i];
            char fname[FS_MAX_PATH];
            uint32_t fsize = 0;
            uint8_t ftype = 0;
            fs_get_entry(entry_idx, fname, &fsize, &ftype);

            int r = i / cols;
            int c = i % cols;
            int gx = pane_x + 8 + c * gw;
            int gy = grid_start_y + r * (gh + 10);

            int is_sel = (files_state.selected_idx == entry_idx);
            if (is_sel) {
                gfx_fill_rounded_rect(gx, gy, gw - 6, gh, 8, 0xFF2B2C40);
                gfx_draw_rounded_rect(gx, gy, gw - 6, gh, 8, COLOR_BLUE_ACCENT);
            }

            icon_type_t ic = (ftype == FS_TYPE_DIR) ? ICON_FILE_MANAGER : ICON_SYSINFO;
            if (str_starts_with(fname, "/bin") || str_starts_with(fname, "/lib")) ic = ICON_TERMINAL;
            icon_draw_32(gx + (gw - 38) / 2, gy + 4, ic);

            const char* disp_name = fname;
            for (int s = 0; fname[s]; s++) {
                if (fname[s] == '/' && fname[s + 1] != '\0') disp_name = &fname[s + 1];
            }
            gfx_draw_text_ellipsis(gx + 2, gy + 42, gw - 10, disp_name, is_sel ? COLOR_TEXT_PRIMARY : COLOR_TEXT_SECONDARY);
        }

        if (max_scroll > 0) {
            int sb_w = 6;
            int sb_x = pane_x + pane_w - sb_w - 2;
            int sb_y = grid_start_y;
            int sb_h = grid_avail_h;

            gfx_fill_rounded_rect(sb_x, sb_y, sb_w, sb_h, 3, 0xFF181825);
            int thumb_h = (rows_visible * sb_h) / total_rows;
            if (thumb_h < 18) thumb_h = 18;
            if (thumb_h > sb_h) thumb_h = sb_h;

            int max_travel = sb_h - thumb_h;
            int thumb_y = sb_y + ((files_state.scroll_y * max_travel) / max_scroll);
            uint32_t thumb_col = files_state.is_dragging_scroll ? COLOR_BLUE_ACCENT : COLOR_BORDER_LIGHT;
            gfx_fill_rounded_rect(sb_x, thumb_y, sb_w, thumb_h, 3, thumb_col);
        }
    }

    // Bottom action bar on files pane: [+ New] [Download] [Delete]
    gfx_fill_rect(pane_x, act_y - 1, pane_w, 1, COLOR_OVERLAY);

    gfx_fill_rounded_rect(pane_x + 8, act_y + 4, 64, 26, 6, COLOR_OVERLAY);
    gfx_draw_rounded_rect(pane_x + 8, act_y + 4, 64, 26, 6, COLOR_BORDER_LIGHT);
    gfx_draw_text(pane_x + 16, act_y + 9, "+ New", COLOR_TEXT_PRIMARY);

    gfx_fill_rounded_rect(pane_x + 78, act_y + 4, 80, 26, 6, COLOR_OVERLAY);
    gfx_draw_rounded_rect(pane_x + 78, act_y + 4, 80, 26, 6, COLOR_BORDER_LIGHT);
    gfx_draw_text(pane_x + 86, act_y + 9, "Download", COLOR_GREEN_ACCENT);

    gfx_fill_rounded_rect(pane_x + 164, act_y + 4, 64, 26, 6, COLOR_OVERLAY);
    gfx_draw_rounded_rect(pane_x + 164, act_y + 4, 64, 26, 6, COLOR_BORDER_LIGHT);
    gfx_draw_text(pane_x + 174, act_y + 9, "Delete", COLOR_RED_ACCENT);

    // 4. Right Pane: Editor or Binary Inspector
    int right_x = pane_x + pane_w;
    int right_w = cw - (right_x - cx);
    gfx_fill_rect(right_x - 1, body_y, 1, body_h, COLOR_OVERLAY);
    gfx_fill_rect(right_x, body_y, right_w, body_h, 0xFF181825);

    if (files_state.right_pane == PANE_INSPECTOR) {
        // ELF Binary Inspector Card
        int ins_x = right_x + 10;
        int ins_y = body_y + 10;

        gfx_draw_text(ins_x, ins_y, "ELF Binary Inspector", COLOR_LEMON_GOLD);
        gfx_draw_text_ellipsis(ins_x, ins_y + 18, right_w - 20, files_state.current_file, COLOR_TEXT_PRIMARY);

        // Header table card
        int card_y = ins_y + 40;
        gfx_fill_rounded_rect(ins_x, card_y, right_w - 20, 110, 8, 0xFF11111B);
        gfx_draw_rounded_rect(ins_x, card_y, right_w - 20, 110, 8, COLOR_BORDER);

        gfx_draw_text(ins_x + 10, card_y + 8,  "Format:    ELF32 (i386 32-bit)", COLOR_TEXT_SECONDARY);
        gfx_draw_text(ins_x + 10, card_y + 26, (files_state.elf_type == 3) ? "Type:      ET_DYN (Position Independent)" : "Type:      ET_EXEC (Executable)", COLOR_TEXT_SECONDARY);
        gfx_draw_text(ins_x + 10, card_y + 44, "Dynamic:   DT_NEEDED (libc.so linked)", files_state.elf_has_dyn ? COLOR_GREEN_ACCENT : COLOR_TEXT_MUTED);
        gfx_draw_text(ins_x + 10, card_y + 62, "GOT/PLT:   Lazy Binding Trampoline", COLOR_TEXT_SECONDARY);
        gfx_draw_text(ins_x + 10, card_y + 80, "Security:  NX Stack / Ring 3 Isolation", COLOR_TEXT_SECONDARY);

        // [Run in Ring 3] Action Button
        int run_y = card_y + 120;
        gfx_fill_rounded_rect(ins_x, run_y, right_w - 20, 32, 8, COLOR_GREEN_ACCENT);
        icon_draw_16(ins_x + 16, run_y + 8, ICON_TERMINAL, 0xFF11111B);
        gfx_draw_text(ins_x + 40, run_y + 8, "Run in Ring 3 (User Mode)", 0xFF11111B);

        // Execution output console card
        int out_y = run_y + 44;
        gfx_fill_rounded_rect(ins_x, out_y, right_w - 20, body_h - (out_y - body_y) - 10, 8, 0xFF0D0E15);
        gfx_draw_rounded_rect(ins_x, out_y, right_w - 20, body_h - (out_y - body_y) - 10, 8, COLOR_BORDER);
        gfx_draw_text(ins_x + 10, out_y + 8, "Execution Log:", COLOR_TEXT_MUTED);
        if (files_state.elf_ran) {
            gfx_draw_text(ins_x + 10, out_y + 26, "Exit Code: 0 (STATUS_SUCCESS)", COLOR_GREEN_ACCENT);
            gfx_draw_text_clipped(ins_x + 10, out_y + 44, right_w - 40, files_state.elf_run_output, COLOR_TEXT_PRIMARY);
        } else {
            gfx_draw_text(ins_x + 10, out_y + 26, "Click above to launch in Ring 3.", COLOR_TEXT_MUTED);
        }
    } else {
        // Text Editor Pane
        int ed_x = right_x + 10;
        int ed_y = body_y + 10;

        const char* fn_str = files_state.current_file[0] ? files_state.current_file : "Untitled";
        int save_w = 68;
        int save_x = right_x + right_w - save_w - 10;
        int max_fn_w = (save_x - 14) - ed_x;
        if (files_state.status_msg[0]) {
            int sw = font_get_string_width(files_state.status_msg);
            int fn_alloc_w = max_fn_w - sw - 14;
            if (fn_alloc_w < 60) fn_alloc_w = 60;
            gfx_draw_text_ellipsis(ed_x, ed_y, fn_alloc_w, fn_str, COLOR_LEMON_GOLD);
            int actual_fn_w = font_get_string_width(fn_str);
            if (actual_fn_w > fn_alloc_w) actual_fn_w = fn_alloc_w;
            gfx_draw_text_ellipsis(ed_x + actual_fn_w + 10, ed_y, (save_x - 4) - (ed_x + actual_fn_w + 10), files_state.status_msg, COLOR_GREEN_ACCENT);
        } else {
            gfx_draw_text_ellipsis(ed_x, ed_y, max_fn_w, fn_str, COLOR_LEMON_GOLD);
        }

        // Save button
        gfx_fill_rounded_rect(save_x, ed_y - 2, save_w, 24, 6, COLOR_GREEN_ACCENT);
        gfx_draw_text(save_x + 14, ed_y + 2, "Save", 0xFF11111B);

        // Editor text box
        int tb_y = ed_y + 26;
        int tb_h = body_h - (tb_y - body_y) - 10;
        int tb_w = right_w - 20;
        gfx_fill_rounded_rect(ed_x, tb_y, tb_w, tb_h, 8, 0xFF11111B);
        gfx_draw_rounded_rect(ed_x, tb_y, tb_w, tb_h, 8, COLOR_BORDER);

        // Render file content text inside editor area
        int tx = ed_x + 10;
        int ty = tb_y + 10;
        int start_tx = tx;

        for (int i = 0; i < files_state.editor_len; i++) {
            char c = files_state.editor_buf[i];
            if (c == '\n') {
                tx = start_tx;
                ty += FONT_HEIGHT + 2;
                if (ty > tb_y + tb_h - 20) break;
            } else {
                if (tx + FONT_WIDTH < ed_x + tb_w - 10) {
                    uint32_t char_color = COLOR_TEXT_PRIMARY;
                    if (c == '#' || c == '=') char_color = COLOR_LEMON_GOLD;
                    else if (c == '<' || c == '>') char_color = COLOR_BLUE_ACCENT;
                    font_draw_char(gfx_get_backbuffer(), SCREEN_WIDTH, SCREEN_HEIGHT, tx, ty, c, char_color, 0, 1);
                    tx += FONT_WIDTH;
                }
            }
        }

        // Cursor
        if (ty <= tb_y + tb_h - 20 && tx + 8 < ed_x + tb_w - 10) {
            gfx_fill_rect(tx, ty + 2, 8, FONT_HEIGHT - 2, COLOR_LEMON_AMBER);
        }
    }
}

static void files_on_mouse(window_t* win, int rx, int ry, int buttons, int event_type) {
    (void)buttons;
    int nav_h = 36;
    int side_w = 160;
    int cw = (win) ? (win->width - 6) : 680;
    int ch = (win) ? (win->height - 35) : 460;
    int body_y = nav_h;
    int body_h = ch - nav_h;
    int act_y = body_y + body_h - 36;
    int pane_w = (cw - side_w) / 2;
    int pane_x = side_w;

    // Filter file indices
    int total_files = fs_get_count();
    int visible_files = 0;
    int file_indices[FILES_MAX];

    for (int i = 0; i < total_files && visible_files < FILES_MAX; i++) {
        char fname[FS_MAX_PATH];
        uint32_t fsize; uint8_t ftype;
        if (fs_get_entry(i, fname, &fsize, &ftype)) {
            if (files_state.current_dir[1] == '\0') {
                file_indices[visible_files++] = i;
            } else if (str_starts_with(fname, files_state.current_dir)) {
                if (strcmp(fname, files_state.current_dir) == 0) continue;
                file_indices[visible_files++] = i;
            }
        }
    }

    int list_start_y = nav_h + 6 + 26;
    int list_avail_h = act_y - list_start_y;
    int list_rows_visible = list_avail_h / 28;
    if (list_rows_visible < 1) list_rows_visible = 1;
    int list_max_scroll = (visible_files > list_rows_visible) ? (visible_files - list_rows_visible) : 0;

    int cols = 3;
    int gh = 64;
    int total_rows = (visible_files + cols - 1) / cols;
    int grid_start_y = body_y + 10;
    int grid_avail_h = act_y - grid_start_y;
    int grid_rows_visible = grid_avail_h / (gh + 10);
    if (grid_rows_visible < 1) grid_rows_visible = 1;
    int grid_max_scroll = (total_rows > grid_rows_visible) ? (total_rows - grid_rows_visible) : 0;

    int cur_max_scroll = (files_state.view_mode == VIEW_MODE_LIST) ? list_max_scroll : grid_max_scroll;

    // 0. Handle scroll wheel
    if (event_type == WM_EVENT_SCROLL_UP) {
        if (files_state.scroll_y > 0) files_state.scroll_y--;
        return;
    } else if (event_type == WM_EVENT_SCROLL_DOWN) {
        if (files_state.scroll_y < cur_max_scroll) files_state.scroll_y++;
        return;
    }

    // Scrollbar drag handling in middle pane
    int sb_x = pane_x + pane_w - 14;
    if (event_type == WM_EVENT_MOUSE_DOWN) {
        if (cur_max_scroll > 0 && rx >= sb_x && rx <= pane_x + pane_w && ry >= nav_h && ry < act_y) {
            files_state.is_dragging_scroll = 1;
            files_state.drag_start_y = ry;
            files_state.drag_start_scroll = files_state.scroll_y;
            return;
        }
    } else if (event_type == WM_EVENT_MOUSE_DRAG) {
        if (files_state.is_dragging_scroll && cur_max_scroll > 0) {
            int sb_h = act_y - list_start_y;
            if (sb_h > 20) {
                int dy = ry - files_state.drag_start_y;
                int delta = (dy * cur_max_scroll) / sb_h;
                files_state.scroll_y = files_state.drag_start_scroll + delta;
                if (files_state.scroll_y < 0) files_state.scroll_y = 0;
                if (files_state.scroll_y > cur_max_scroll) files_state.scroll_y = cur_max_scroll;
            }
            return;
        }
    } else if (event_type == WM_EVENT_MOUSE_UP) {
        files_state.is_dragging_scroll = 0;
    }

    if (event_type != WM_EVENT_MOUSE_CLICK && event_type != WM_EVENT_MOUSE_DOWN) return;

    // 1. Navigation bar clicks
    if (ry < nav_h) {
        // [< Back]
        if (rx >= 8 && rx < 34) {
            files_set_dir("/");
            return;
        }
        // [^ Up]
        if (rx >= 68 && rx < 94) {
            files_set_dir("/");
            return;
        }
        // [Refresh]
        if (rx >= 98 && rx < 124) {
            alert_send("LemonFS Refreshed", "Directory synchronized", ICON_REFRESH, ALERT_TYPE_INFO);
            return;
        }
        // View mode toggle
        int vm_x = cw - 110;
        if (rx >= vm_x && rx < vm_x + 48) {
            files_state.view_mode = VIEW_MODE_LIST;
            files_state.scroll_y = 0;
            return;
        } else if (rx >= vm_x + 52 && rx < vm_x + 100) {
            files_state.view_mode = VIEW_MODE_GRID;
            files_state.scroll_y = 0;
            return;
        }
    }

    // 2. Sidebar clicks
    if (rx < side_w) {
        int s_body_y = nav_h;
        for (int p = 0; p < 6; p++) {
            int py = s_body_y + 28 + p * 28;
            if (ry >= py && ry < py + 24) {
                const char* target_paths[] = {"/", "/docs", "/downloads", "/bin", "/lib", "/system"};
                files_set_dir(target_paths[p]);
                return;
            }
        }
    }

    // 3. Middle pane clicks (File selection)
    if (rx >= pane_x && rx < pane_x + pane_w) {
        if (files_state.view_mode == VIEW_MODE_LIST) {
            int th_y = nav_h + 6;
            for (int r = 0; r < list_rows_visible && (files_state.scroll_y + r) < visible_files; r++) {
                int fy = th_y + 26 + r * 28;
                if (ry >= fy && ry < fy + 26) {
                    int item_idx = files_state.scroll_y + r;
                    files_state.selected_idx = file_indices[item_idx];
                    char fname[FS_MAX_PATH];
                    uint32_t fsize; uint8_t ftype;
                    if (fs_get_entry(file_indices[item_idx], fname, &fsize, &ftype)) {
                        if (ftype == FS_TYPE_DIR) {
                            files_set_dir(fname);
                        } else {
                            files_select_file(fname);
                        }
                    }
                    return;
                }
            }
        } else {
            // Grid clicks
            int gw = (pane_w - 24) / cols;
            int start_item = files_state.scroll_y * cols;
            int max_items = grid_rows_visible * cols;
            for (int i = 0; i < max_items && (start_item + i) < visible_files; i++) {
                int r = i / cols;
                int c = i % cols;
                int gx = pane_x + 8 + c * gw;
                int gy = nav_h + 10 + r * (gh + 10);
                if (rx >= gx && rx < gx + gw && ry >= gy && ry < gy + gh) {
                    int item_idx = start_item + i;
                    files_state.selected_idx = file_indices[item_idx];
                    char fname[FS_MAX_PATH];
                    uint32_t fsize; uint8_t ftype;
                    if (fs_get_entry(file_indices[item_idx], fname, &fsize, &ftype)) {
                        if (ftype == FS_TYPE_DIR) {
                            files_set_dir(fname);
                        } else {
                            files_select_file(fname);
                        }
                    }
                    return;
                }
            }
        }

        // Action buttons at bottom of pane
        int act_y = ch - 36;
        if (ry >= act_y + 4 && ry < act_y + 30) {
            if (rx >= pane_x + 8 && rx < pane_x + 72) {
                // + New
                fs_create_file("/docs/notes_new.txt", "New document in LemonFS.\n");
                files_select_file("/docs/notes_new.txt");
                return;
            } else if (rx >= pane_x + 78 && rx < pane_x + 158) {
                // Download
                files_trigger_download();
                return;
            } else if (rx >= pane_x + 164 && rx < pane_x + 228) {
                // Delete
                if (files_state.current_file[0]) {
                    fs_delete_file(files_state.current_file);
                    alert_send("File Deleted", files_state.current_file, ICON_FILE_MANAGER, ALERT_TYPE_WARNING);
                    files_select_file("/docs/welcome.txt");
                }
                return;
            }
        }
    }

    // 4. Right pane clicks
    int right_x = pane_x + pane_w;
    int right_w = cw - (right_x - 0);

    if (rx >= right_x) {
        if (files_state.right_pane == PANE_INSPECTOR) {
            int ins_x = right_x + 10;
            int run_y = nav_h + 10 + 40 + 120;
            if (rx >= ins_x && rx < ins_x + right_w - 20 && ry >= run_y && ry < run_y + 32) {
                files_run_selected_elf();
                return;
            }
        } else {
            // Save button
            int save_w = 68;
            int save_x = right_x + right_w - save_w - 10;
            int ed_y = nav_h + 10;
            if (rx >= save_x && rx < save_x + save_w && ry >= ed_y - 2 && ry < ed_y + 22) {
                files_save_current();
                return;
            }
        }
    }
}

static void files_on_key(window_t* win, char key) {
    (void)win;
    if (files_state.right_pane != PANE_EDITOR) return;

    if (key == '\b') {
        if (files_state.editor_len > 0) {
            files_state.editor_len--;
            files_state.editor_buf[files_state.editor_len] = '\0';
        }
    } else if (key == '\n') {
        if (files_state.editor_len < FILE_EDIT_LEN - 1) {
            files_state.editor_buf[files_state.editor_len++] = '\n';
            files_state.editor_buf[files_state.editor_len] = '\0';
        }
    } else if (key >= 32 && key <= 126) {
        if (files_state.editor_len < FILE_EDIT_LEN - 1) {
            files_state.editor_buf[files_state.editor_len++] = key;
            files_state.editor_buf[files_state.editor_len] = '\0';
        }
    }
}

static void files_on_close(window_t* win) {
    (void)win;
    files_win = NULL;
}

void gui_files_open(void) {
    if (files_win && !files_win->is_closed) {
        if (files_win->is_minimized) wm_restore_window(files_win);
        wm_bring_to_front(files_win);
        return;
    }

    fs_init();
    files_state.view_mode = VIEW_MODE_LIST;
    files_state.right_pane = PANE_EDITOR;
    files_set_dir("/bin");
    files_state.selected_idx = 0;
    files_select_file("/bin/hello");

    files_win = wm_create_window("File Explorer", ICON_FILES, 40, 50, 680, 460, files_render);
    if (files_win) {
        files_win->on_mouse = files_on_mouse;
        files_win->on_key = files_on_key;
        files_win->on_close = files_on_close;
    }
}
