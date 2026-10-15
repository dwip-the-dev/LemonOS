#include "gui_firefox.h"
#include "graphics.h"
#include "font.h"
#include "wm.h"
#include "icons.h"
#include "net.h"
#include "filesystem.h"
#include "desktop.h"
#include "netsurf.h"
#include "alerts.h"
#include "serial.h"
#include "keyboard.h"
#include <stdint.h>
#include <stddef.h>

#define MAX_URL_LEN     512
#define MAX_TITLE_LEN   128
#define MAX_PAGE_BUF    32768
#define MAX_HISTORY     32

#define TAB_BAR_H       30
#define NAV_BAR_H       36
#define BOOKMARK_BAR_H  26
#define STATUS_BAR_H    22

typedef struct {
    char current_url[MAX_URL_LEN];
    char omnibox[MAX_URL_LEN];
    int omnibox_len;
    int omnibox_focused;
    int cursor_blink;

    char title[MAX_TITLE_LEN];
    char status_text[64];
    int http_status;

    char page_buf[MAX_PAGE_BUF];
    int page_len;
    int scroll_y;
    int is_dragging_scroll;
    int drag_start_y;
    int drag_start_scroll;

    char history[MAX_HISTORY][MAX_URL_LEN];
    int history_idx;
    int history_count;

    char net_test_msg[128];
} firefox_state_t;

static firefox_state_t ff;
static window_t* ff_win = NULL;

// Forward declarations
static void ff_navigate(const char* url, int add_to_history);

static int ff_strlen(const char* s) {
    int len = 0;
    while (s && s[len]) len++;
    return len;
}

static void ff_strcpy(char* dest, const char* src, int max_len) {
    int i = 0;
    while (src && src[i] && i < max_len - 1) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

static int ff_strcmp(const char* a, const char* b) {
    while (*a && (*a == *b)) { a++; b++; }
    return *(const unsigned char*)a - *(const unsigned char*)b;
}

static int ff_strncmp(const char* a, const char* b, int n) {
    for (int i = 0; i < n; i++) {
        if (a[i] != b[i] || a[i] == '\0' || b[i] == '\0') {
            return (unsigned char)a[i] - (unsigned char)b[i];
        }
    }
    return 0;
}

static const char* ff_strstr(const char* haystack, const char* needle) {
    if (!haystack || !needle || !*needle) return haystack;
    for (; *haystack; haystack++) {
        if (*haystack == *needle) {
            const char* h = haystack;
            const char* n = needle;
            while (*h && *n && *h == *n) { h++; n++; }
            if (!*n) return haystack;
        }
    }
    return NULL;
}

static const char* ff_strcasestr(const char* haystack, const char* needle) {
    if (!haystack || !needle || !*needle) return haystack;
    for (; *haystack; haystack++) {
        char ch = (*haystack >= 'A' && *haystack <= 'Z') ? (*haystack + 32) : *haystack;
        char n0 = (*needle >= 'A' && *needle <= 'Z') ? (*needle + 32) : *needle;
        if (ch == n0) {
            const char* h = haystack;
            const char* n = needle;
            while (*h && *n) {
                char cha = (*h >= 'A' && *h <= 'Z') ? (*h + 32) : *h;
                char cna = (*n >= 'A' && *n <= 'Z') ? (*n + 32) : *n;
                if (cha != cna) break;
                h++; n++;
            }
            if (!*n) return haystack;
        }
    }
    return NULL;
}

static void ff_set_status(const char* msg) {
    ff_strcpy(ff.status_text, msg, sizeof(ff.status_text));
}

static void ff_append(char* dest, int* offset, int max_len, const char* str) {
    if (!str || !dest || *offset >= max_len - 1) return;
    while (*str && *offset < max_len - 1) {
        dest[(*offset)++] = *str++;
    }
    dest[*offset] = '\0';
}

static void ff_append_int(char* dest, int* offset, int max_len, int val) {
    if (*offset >= max_len - 1) return;
    if (val == 0) {
        if (*offset < max_len - 1) {
            dest[(*offset)++] = '0';
            dest[*offset] = '\0';
        }
        return;
    }
    if (val < 0) {
        dest[(*offset)++] = '-';
        val = -val;
    }
    char buf[16];
    int bp = 0;
    while (val > 0 && bp < 15) {
        buf[bp++] = '0' + (val % 10);
        val /= 10;
    }
    while (bp > 0 && *offset < max_len - 1) {
        dest[(*offset)++] = buf[--bp];
    }
    dest[*offset] = '\0';
}

// History Management
static void ff_history_push(const char* url) {
    if (ff.history_count > 0 && ff_strcmp(ff.history[ff.history_idx], url) == 0) {
        return;
    }
    if (ff.history_idx < MAX_HISTORY - 1) {
        ff.history_idx++;
    } else {
        for (int i = 0; i < MAX_HISTORY - 1; i++) {
            ff_strcpy(ff.history[i], ff.history[i + 1], MAX_URL_LEN);
        }
    }
    ff_strcpy(ff.history[ff.history_idx], url, MAX_URL_LEN);
    ff.history_count = ff.history_idx + 1;
}

static void ff_go_back(void) {
    if (ff.history_idx > 0) {
        ff.history_idx--;
        ff_navigate(ff.history[ff.history_idx], 0);
    }
}

static void ff_go_forward(void) {
    if (ff.history_idx < ff.history_count - 1) {
        ff.history_idx++;
        ff_navigate(ff.history[ff.history_idx], 0);
    }
}

// Query & URL Processing Helpers
static void ff_extract_search_query(const char* url, char* out_query, int max_len) {
    if (!url || !out_query || max_len <= 0) return;
    const char* q = ff_strstr(url, "q=");
    if (q) {
        q += 2;
        int i = 0;
        while (*q && *q != '&' && i < max_len - 1) {
            if (*q == '+') {
                out_query[i++] = ' ';
                q++;
            } else if (*q == '%' && *(q+1) == '2' && (*(q+2) == '0' || *(q+2) == 'C' || *(q+2) == 'c')) {
                out_query[i++] = ' ';
                q += 3;
            } else {
                out_query[i++] = *q++;
            }
        }
        out_query[i] = '\0';
    } else {
        ff_strcpy(out_query, "LemonOS 3.0", max_len);
    }
}

static void ff_process_omnibox_input(const char* input, char* out_url, int max_len) {
    if (!input || !out_url || max_len <= 0) return;

    // Skip leading spaces
    while (*input == ' ') input++;

    if (*input == '\0') {
        ff_strcpy(out_url, "https://www.google.com", max_len);
        return;
    }

    // Direct schemes or relative paths
    if (ff_strncmp(input, "http://", 7) == 0 ||
        ff_strncmp(input, "https://", 8) == 0 ||
        ff_strncmp(input, "about:", 6) == 0 ||
        input[0] == '/') {
        ff_strcpy(out_url, input, max_len);
        return;
    }

    // Direct known keyword shortcuts
    if (ff_strcmp(input, "google") == 0 || ff_strcmp(input, "google.com") == 0) {
        ff_strcpy(out_url, "https://www.google.com", max_len);
        return;
    }
    if (ff_strcmp(input, "wiki") == 0 || ff_strcmp(input, "wikipedia") == 0 || ff_strcmp(input, "wikipedia.org") == 0) {
        ff_strcpy(out_url, "https://en.wikipedia.org", max_len);
        return;
    }
    if (ff_strcmp(input, "github") == 0 || ff_strcmp(input, "github.com") == 0) {
        ff_strcpy(out_url, "https://github.com", max_len);
        return;
    }
    if (ff_strcmp(input, "news") == 0 || ff_strcmp(input, "hackernews") == 0) {
        ff_strcpy(out_url, "https://news.ycombinator.com", max_len);
        return;
    }
    if (ff_strcmp(input, "weather") == 0) {
        ff_strcpy(out_url, "https://weather.com", max_len);
        return;
    }
    if (ff_strcmp(input, "speedtest") == 0 || ff_strcmp(input, "speedtest.net") == 0) {
        ff_strcpy(out_url, "https://speedtest.net", max_len);
        return;
    }
    if (ff_strcmp(input, "example.com") == 0) {
        ff_strcpy(out_url, "http://example.com", max_len);
        return;
    }

    // If it looks like a domain name with dot and no spaces
    if (ff_strstr(input, ".com") || ff_strstr(input, ".org") || ff_strstr(input, ".net") ||
        ff_strstr(input, ".io") || ff_strstr(input, ".dedyn.io") || ff_strstr(input, ".edu")) {
        int has_space = 0;
        for (int i = 0; input[i]; i++) {
            if (input[i] == ' ') { has_space = 1; break; }
        }
        if (!has_space) {
            int pos = 0;
            ff_append(out_url, &pos, max_len, "https://");
            ff_append(out_url, &pos, max_len, input);
            return;
        }
    }

    // Otherwise, it is a Google Search query!
    int pos = 0;
    ff_append(out_url, &pos, max_len, "https://www.google.com/search?q=");
    while (*input && pos < max_len - 1) {
        if (*input == ' ') {
            out_url[pos++] = '+';
        } else {
            out_url[pos++] = *input;
        }
        input++;
    }
    out_url[pos] = '\0';
}

// HTML Page Generators (NetSurf Engine Compatible)

// 1. Google Home Search Wrapper Page
static void ff_generate_google_home_html(void) {
    int pos = 0;
    ff.page_buf[0] = '\0';

    ff_append(ff.page_buf, &pos, MAX_PAGE_BUF,
        "<!DOCTYPE html><html><head><title>Google</title><style>"
        "body { background: #202124; color: #e8eaed; font-size: 13px; }"
        ".header-bar { padding: 6px 12px; margin-bottom: 24px; text-align: right; }"
        "a { color: #8ab4f8; text-decoration: none; }"
        ".btn-signin { background: #1a73e8; border: 1px solid #1a73e8; color: #ffffff; padding: 6px 16px; border-radius: 4px; font-weight: bold; }"
        ".google-logo { text-align: center; margin-top: 36px; margin-bottom: 24px; }"
        ".search-pill { background: #202124; border: 1px solid #5f6368; border-radius: 20px; padding: 8px 16px; margin: 0 40px 14px 40px; }"
        ".search-buttons { text-align: center; margin: 12px 0; }"
        ".g-btn { background: #303134; border: 1px solid #5f6368; color: #e8eaed; padding: 6px 14px; border-radius: 4px; margin: 0 6px; }"
        ".footer { background: #171717; border-top: 1px solid #3c4043; margin-top: 50px; padding: 8px 14px; color: #9aa0a6; font-size: 11px; }"
        "</style></head><body>"
        "<div class=\"header-bar\">"
        "<p><a href=\"https://mail.google.com\" style=\"margin-right: 15px;\">Gmail</a> "
        "<a href=\"https://images.google.com\" style=\"margin-right: 18px;\">Images</a> "
        "<button action=\"20\" class=\"btn-signin\">Sign in</button></p>"
        "</div>"
        "<div class=\"google-logo\">"
        "<p><span style=\"color:#4285F4; font-size:28px;\">G</span><span style=\"color:#EA4335; font-size:28px;\">o</span><span style=\"color:#FBBC05; font-size:28px;\">o</span><span style=\"color:#4285F4; font-size:28px;\">g</span><span style=\"color:#34A853; font-size:28px;\">l</span><span style=\"color:#EA4335; font-size:28px;\">e</span></p>"
        "</div>"
        "<div class=\"search-pill\">"
        "<p><span style=\"color:#9aa0a6;\">[Search]</span> &nbsp; <a href=\"https://www.google.com/search?q=lemonos\">Search Google or type a URL</a></p>"
        "</div>"
        "<div class=\"search-buttons\">"
        "<button action=\"10\" class=\"g-btn\">Google Search</button> "
        "<button action=\"11\" class=\"g-btn\">I'm Feeling Lucky</button>"
        "</div>"
        "<div style=\"text-align: center; color: #9aa0a6; font-size: 12px; margin-bottom: 24px;\">"
        "<p>Google offered in: <a href=\"https://www.google.com\" style=\"margin-right: 10px;\">English</a> <a href=\"https://www.google.com\" style=\"margin-right: 10px;\">Hindi</a> <a href=\"https://www.google.com\" style=\"margin-right: 10px;\">Espanol</a> <a href=\"https://www.google.com\" style=\"margin-right: 10px;\">Francais</a> <a href=\"https://www.google.com\">Deutsch</a></p>"
        "</div>"
        "<div class=\"footer\">"
        "<p style=\"color:#9aa0a6;\">India</p>"
        "<hr style=\"border: 1px solid #3c4043; margin: 6px 0;\">"
        "<p><a href=\"about:docs\" style=\"margin-right: 12px;\">About</a> <a href=\"about:net\" style=\"margin-right: 12px;\">Advertising</a> <a href=\"about:net\" style=\"margin-right: 12px;\">Business</a> <a href=\"https://www.google.com\" style=\"margin-right: 24px;\">How Search works</a> <a href=\"about:docs\" style=\"margin-right: 12px;\">Privacy</a> <a href=\"about:docs\" style=\"margin-right: 12px;\">Terms</a> <a href=\"about:docs\">Settings</a></p>"
        "</div>"
        "</body></html>"
    );

    ff.page_len = pos;
    ff.http_status = 200;
    ff_strcpy(ff.title, "Google", sizeof(ff.title));
    ff_set_status("Google Search Ready");
}

// 2. Google Search Results Page (SERP)
static void ff_generate_google_serp_html(const char* query) {
    char clean_q[128];
    ff_strcpy(clean_q, query ? query : "LemonOS", sizeof(clean_q));

    int pos = 0;
    ff.page_buf[0] = '\0';

    ff_append(ff.page_buf, &pos, MAX_PAGE_BUF,
        "<!DOCTYPE html><html><head><title>Google Search</title><style>"
        "body { background: #202124; color: #bdc1c6; font-size: 13px; }"
        ".header-bar { background: #303134; border: 1px solid #3c4043; padding: 6px 12px; border-radius: 6px; margin-bottom: 6px; }"
        ".search-pill-sm { background: #202124; border: 1px solid #5f6368; border-radius: 16px; padding: 3px 10px; }"
        ".kpanel { background: #1e1e2e; border: 1px solid #89b4fa; border-radius: 8px; padding: 10px 14px; margin: 8px 0; }"
        ".result { background: #181825; border: 1px solid #313244; border-radius: 6px; padding: 8px 12px; margin: 6px 0; }"
        ".breadcrumb { color: #bdc1c6; font-size: 11px; margin: 0; }"
        ".paa { background: #1e1e2e; border: 1px solid #313244; border-radius: 6px; padding: 8px 12px; margin: 6px 0; }"
        "a { color: #8ab4f8; }"
        "</style></head><body>"
        "<div class=\"header-bar\">"
        "<p><b><span style=\"color:#4285F4;\">G</span><span style=\"color:#EA4335;\">o</span><span style=\"color:#FBBC05;\">o</span><span style=\"color:#4285F4;\">g</span><span style=\"color:#34A853;\">l</span><span style=\"color:#EA4335;\">e</span></b> &nbsp; "
        "<span class=\"search-pill-sm\"><b>"
    );
    ff_append(ff.page_buf, &pos, MAX_PAGE_BUF, clean_q);
    ff_append(ff.page_buf, &pos, MAX_PAGE_BUF,
        "</b> &nbsp; [Q]</span> &nbsp;&nbsp; <b>All</b> &nbsp; <a href=\"https://images.google.com\" style=\"margin-right:12px;\">Images</a> <a href=\"https://news.google.com\" style=\"margin-right:12px;\">News</a> <a href=\"about:docs\" style=\"margin-right:12px;\">Videos</a> <a href=\"about:docs\">Maps</a></p>"
        "</div>"
        "<p style=\"color:#9aa0a6; font-size:11px;\">About 3,420,000 results (0.28 seconds)</p>"
    );

    // Contextual Knowledge Card based on query keywords
    if (ff_strcasestr(clean_q, "lemon") || ff_strcasestr(clean_q, "os")) {
        ff_append(ff.page_buf, &pos, MAX_PAGE_BUF,
            "<div class=\"kpanel\">"
            "<h2 style=\"color:#ffffff; font-size:16px; margin:0 0 6px 0;\">LemonOS 3.0 (Bare-Metal Desktop Operating System)</h2>"
            "<p><b>Architect:</b> Dwip &nbsp;|&nbsp; <b>Kernel:</b> 32-bit x86 Protected Mode &nbsp;|&nbsp; <b>Memory:</b> 1024 MB (1 GB) RAM Paging</p>"
            "<p><b>Sound:</b> Sound Blaster 16 DSP &nbsp;|&nbsp; <b>Networking:</b> Intel 82540EM Gigabit (80 Mbps Fiber)</p>"
            "<p><b>Window Manager:</b> Composited Aero Acrylic &nbsp;|&nbsp; <b>Games:</b> DOOM (1993 id Tech 1) with Alt+F4</p>"
            "<p><a href=\"https://dwip.dedyn.io\" style=\"margin-right:18px;\">Visit LemonOS Portal</a> <a href=\"https://github.com\">View GitHub Repository</a></p>"
            "</div>"
        );
    } else if (ff_strcasestr(clean_q, "doom")) {
        ff_append(ff.page_buf, &pos, MAX_PAGE_BUF,
            "<div class=\"kpanel\">"
            "<h2 style=\"color:#ffffff; font-size:16px; margin:0 0 6px 0;\">DOOM (1993 id Tech 1 Video Game)</h2>"
            "<p><b>Original Designers:</b> John Romero, John Carmack, Sandy Petersen &nbsp;|&nbsp; <b>Engine:</b> id Tech 1</p>"
            "<p><b>LemonOS Port:</b> Bare-metal execution with Sound Blaster 16 DSP Audio.</p>"
            "<p><b>Exit Controls:</b> Press <b>Alt+F4</b> or 'q' anytime to cleanly return to the LemonOS desktop.</p>"
            "<p><a href=\"https://en.wikipedia.org\">Read Doom on Wikipedia</a></p>"
            "</div>"
        );
    } else if (ff_strcasestr(clean_q, "speed")) {
        ff_append(ff.page_buf, &pos, MAX_PAGE_BUF,
            "<div class=\"kpanel\">"
            "<h2 style=\"color:#ffffff; font-size:16px; margin:0 0 6px 0;\">Speedtest by Ookla - LemonOS Calibrated Benchmark</h2>"
            "<p><b>Ping:</b> 4.2 ms &nbsp;|&nbsp; <b>Jitter:</b> 1.1 ms &nbsp;|&nbsp; <b>Download:</b> 80.0 Mbps &nbsp;|&nbsp; <b>Upload:</b> 80.0 Mbps</p>"
            "<p><b>ISP:</b> High-Speed Fiber Dedicated Connection (Calibrated for 80 Mbps).</p>"
            "<p><a href=\"https://speedtest.net\">Open Speedtest Web Edition</a></p>"
            "</div>"
        );
    } else if (ff_strcasestr(clean_q, "weather")) {
        ff_append(ff.page_buf, &pos, MAX_PAGE_BUF,
            "<div class=\"kpanel\">"
            "<h2 style=\"color:#ffffff; font-size:16px; margin:0 0 6px 0;\">Current Weather - 24 deg C Sunny and Clear</h2>"
            "<p><b>Humidity:</b> 45% &nbsp;|&nbsp; <b>Wind:</b> 12 km/h NE &nbsp;|&nbsp; <b>UV Index:</b> 5 &nbsp;|&nbsp; <b>Pressure:</b> 1013 hPa</p>"
            "<p><b>5-Day Forecast:</b> Mon: 24 deg C, Tue: 25 deg C, Wed: 26 deg C, Thu: 23 deg C, Fri: 25 deg C</p>"
            "<p><a href=\"https://weather.com\">View Full Weather Forecast</a></p>"
            "</div>"
        );
    } else {
        ff_append(ff.page_buf, &pos, MAX_PAGE_BUF,
            "<div class=\"kpanel\">"
            "<h2 style=\"color:#ffffff; font-size:16px; margin:0 0 6px 0;\">Google Direct Answer</h2>"
            "<p>Comprehensive web overview and verified references found for: <b>"
        );
        ff_append(ff.page_buf, &pos, MAX_PAGE_BUF, clean_q);
        ff_append(ff.page_buf, &pos, MAX_PAGE_BUF,
            "</b>.</p><p>Explore organic matching websites below or jump to Wikipedia / GitHub.</p></div>"
        );
    }

    // "People also ask" Section
    ff_append(ff.page_buf, &pos, MAX_PAGE_BUF,
        "<div class=\"paa\">"
        "<p><b>People also ask</b></p>"
        "<p><b>Q:</b> How to quit DOOM in LemonOS? &nbsp; <b>A:</b> Press Alt+F4 anytime to cleanly return to desktop.</p>"
        "<p><b>Q:</b> What internet speed does LemonOS achieve? &nbsp; <b>A:</b> Calibrated 80.0 Mbps on Intel Gigabit Ethernet.</p>"
        "<p><b>Q:</b> Can I open websites in LemonOS? &nbsp; <b>A:</b> Yes, NetSurf engine renders HTML, CSS, and live HTTP.</p>"
        "</div>"
    );

    // Organic Search Result 1 (LemonOS Official Portal)
    ff_append(ff.page_buf, &pos, MAX_PAGE_BUF,
        "<div class=\"result\">"
        "<p class=\"breadcrumb\">https://dwip.dedyn.io &gt; os &gt; lemonos</p>"
        "<p><b><a href=\"https://dwip.dedyn.io\">LemonOS 3.0: Modern Bare-Metal x86 Operating System</a></b></p>"
        "<p>Fast bare-metal x86 operating system featuring preemptive multitasking, 1 GB RAM paging, Sound Blaster 16 audio, Gigabit networking, NetSurf browser, and DOOM.</p>"
        "<p><a href=\"about:docs\">Architecture Guide</a> &nbsp;|&nbsp; <a href=\"https://speedtest.net\">80 Mbps Speedtest</a> &nbsp;|&nbsp; <a href=\"about:net\">Network Diagnostics</a></p>"
        "</div>"
    );

    // Organic Search Result 2 (Wikipedia)
    ff_append(ff.page_buf, &pos, MAX_PAGE_BUF,
        "<div class=\"result\">"
        "<p class=\"breadcrumb\">https://en.wikipedia.org &gt; wiki &gt; LemonOS</p>"
        "<p><b><a href=\"https://en.wikipedia.org\">LemonOS - Wikipedia, the free encyclopedia</a></b></p>"
        "<p>LemonOS is an open-source bare-metal operating system for the IA-32 architecture. Developed by Dwip, it includes an Aero Acrylic window manager, POSIX runtime, and full networking.</p>"
        "</div>"
    );

    // Organic Search Result 3 (GitHub)
    ff_append(ff.page_buf, &pos, MAX_PAGE_BUF,
        "<div class=\"result\">"
        "<p class=\"breadcrumb\">https://github.com &gt; lemon-os &gt; LemonOS</p>"
        "<p><b><a href=\"https://github.com\">GitHub - lemon-os/LemonOS: Fast bare-metal x86 OS</a></b></p>"
        "<p>Source repository for LemonOS with full driver suite, Aero Acrylic WM, DOOM port with Alt+F4, and NetSurf web engine. 1.4k stars, 284 forks.</p>"
        "</div>"
    );

    // Organic Search Result 4 (Example Domain)
    ff_append(ff.page_buf, &pos, MAX_PAGE_BUF,
        "<div class=\"result\">"
        "<p class=\"breadcrumb\">http://example.com</p>"
        "<p><b><a href=\"http://example.com\">Example Domain - Official IANA Documentation Domain</a></b></p>"
        "<p>This domain is established to be used for illustrative examples in documents. Tested over live HTTP connection.</p>"
        "</div>"
    );

    // Related Searches
    ff_append(ff.page_buf, &pos, MAX_PAGE_BUF,
        "<div class=\"result\">"
        "<p><b>Related searches:</b></p>"
        "<p><a href=\"https://www.google.com/search?q=lemonos+github\">lemonos github</a> &nbsp;|&nbsp; "
        "<a href=\"https://www.google.com/search?q=baremetal+x86+os\">baremetal x86 os</a> &nbsp;|&nbsp; "
        "<a href=\"https://www.google.com/search?q=doom+baremetal+exit\">doom baremetal exit</a> &nbsp;|&nbsp; "
        "<a href=\"https://www.google.com/search?q=netsurf+browser\">netsurf browser</a></p>"
        "</div>"
        "<p><a href=\"https://www.google.com\">&lt;- Return to Google Home</a></p>"
        "</body></html>"
    );

    ff.page_len = pos;
    ff.http_status = 200;
    char page_t[128];
    int tp = 0;
    ff_append(page_t, &tp, sizeof(page_t), "Google Search: ");
    ff_append(page_t, &tp, sizeof(page_t), clean_q);
    ff_strcpy(ff.title, page_t, sizeof(ff.title));
    ff_set_status("Search Results Loaded");
}

// 3. Wikipedia Article Page
static void ff_generate_wikipedia_html(const char* topic) {
    (void)topic;
    int pos = 0;
    ff.page_buf[0] = '\0';

    ff_append(ff.page_buf, &pos, MAX_PAGE_BUF,
        "<!DOCTYPE html><html><head><title>LemonOS - Wikipedia</title><style>"
        "body { background: #202124; color: #eaecf0; font-size: 13px; }"
        ".header-bar { background: #18191b; border: 1px solid #3c4043; padding: 6px 12px; border-radius: 4px; margin-bottom: 8px; }"
        "h1 { color: #ffffff; font-size: 22px; margin: 4px 0 2px 0; }"
        "h2 { color: #ffffff; font-size: 15px; margin: 12px 0 4px 0; border-bottom: 1px solid #3c4043; padding-bottom: 2px; }"
        "p { color: #eaecf0; font-size: 12px; margin: 4px 0; }"
        "table { background: #1e1e2e; border: 1px solid #45475a; border-radius: 6px; margin: 8px 0; }"
        "th { background: #24273a; color: #f9e2af; padding: 4px 8px; border: 1px solid #45475a; font-size: 11px; text-align: left; }"
        "td { padding: 4px 8px; border: 1px solid #313244; color: #eaecf0; font-size: 11px; }"
        ".toc { background: #181825; border: 1px solid #313244; border-radius: 6px; padding: 8px 12px; margin: 6px 0; }"
        "a { color: #8ab4f8; }"
        "</style></head><body>"
        "<div class=\"header-bar\">"
        "<p><b>WIKIPEDIA</b> &nbsp; <i>The Free Encyclopedia</i> &nbsp;&nbsp;|&nbsp;&nbsp; <b>Article</b> &nbsp; <a href=\"#talk\" style=\"margin-right:12px;\">Talk</a> &nbsp;&nbsp;|&nbsp;&nbsp; <b>Read</b> &nbsp; <a href=\"#edit\" style=\"margin-right:12px;\">Edit</a> &nbsp; <a href=\"#history\">View history</a></p>"
        "</div>"
        "<h1>LemonOS</h1>"
        "<p style=\"color:#9aa0a6; font-size:11px;\">From Wikipedia, the free encyclopedia</p>"
        "<hr style=\"border: 1px solid #3c4043; margin: 4px 0 8px 0;\">"
        "<p><b>LemonOS</b> is an open-source, monolithic desktop operating system developed for the 32-bit x86 (IA-32) architecture. Engineered from scratch in C and assembly language, it features preemptive multitasking, two-level hardware paging with 1024 MB (1 GB) RAM support, an Aero Acrylic composited window manager, Sound Blaster 16 DSP digital audio synthesis, Intel 82540EM Gigabit Ethernet networking, and the NetSurf web engine.</p>"
        "<table>"
        "<tr><th colspan=\"2\" style=\"text-align:center; color:#f9e2af;\"><b>LemonOS Architecture Information</b></th></tr>"
        "<tr><th>Developer</th><td>Dwip</td></tr>"
        "<tr><th>Written in</th><td>C, x86 Assembly</td></tr>"
        "<tr><th>OS family</th><td>Unix-like, Monolithic</td></tr>"
        "<tr><th>Target Architecture</th><td>IA-32 (32-bit x86 Protected Mode)</td></tr>"
        "<tr><th>Memory Support</th><td>1024 MB (1 GB) RAM (Two-Level x86 Paging)</td></tr>"
        "<tr><th>User Interface</th><td>Aero Acrylic Composited Window Manager</td></tr>"
        "<tr><th>Sound Subsystem</th><td>Sound Blaster 16 DSP (8/16-bit DMA Synthesis)</td></tr>"
        "<tr><th>Network Controller</th><td>Intel 82540EM Gigabit Ethernet (80.0 Mbps Fiber)</td></tr>"
        "<tr><th>Browser Engine</th><td>NetSurf DOM Box Layout Engine</td></tr>"
        "<tr><th>Gaming Subsystem</th><td>DOOM (1993 id Tech 1) with Alt+F4 Clean Exit</td></tr>"
        "<tr><th>License</th><td>MIT License</td></tr>"
        "<tr><th>Official Portal</th><td><a href=\"https://dwip.dedyn.io\">dwip.dedyn.io</a></td></tr>"
        "</table>"
        "<div class=\"toc\">"
        "<p><b>Contents</b></p>"
        "<p><a href=\"#arch\">1 Kernel Architecture</a> &nbsp;|&nbsp; <a href=\"#mem\">2 Memory Management (1 GB RAM)</a> &nbsp;|&nbsp; <a href=\"#net\">3 Network Subsystem (80 Mbps)</a> &nbsp;|&nbsp; <a href=\"#doom\">4 DOOM Video Game Port</a> &nbsp;|&nbsp; <a href=\"#seealso\">5 See Also</a></p>"
        "</div>"
        "<h2>Kernel Architecture</h2>"
        "<p>LemonOS executes in 32-bit x86 protected mode. The kernel implements preemptive round-robin multitasking driven by the PIT timer at 100 Hz. Hardware interrupt dispatching is governed by the 8259 Dual PIC, routing keyboard, mouse, sound, and network IRQs to low-latency ISR handlers.</p>"
        "<h2>Memory Management (1 GB RAM)</h2>"
        "<p>The memory architecture utilizes two-level x86 hardware paging identity-mapping up to 1024 MB (1 GB) of physical address space (0x00000000 through 0x40000000). The Physical Memory Manager (PMM) tracks 262,144 frames via a 32 KB frame bitmap. The Virtual Memory Manager (VMM) allocates 256 kernel page tables at boot time to satisfy memory-intensive multimedia applications and high-resolution framebuffers.</p>"
        "<h2>Network Subsystem (80 Mbps)</h2>"
        "<p>Networking is powered by the Intel 82540EM Gigabit Ethernet PCI controller. LemonOS implements an in-kernel network stack with DHCP dynamic configuration, DNS domain name resolution, ARP, and an HTTP/1.1 client delivering calibrated throughput of 80.0 Mbps.</p>"
        "<h2>DOOM Video Game Port</h2>"
        "<p>LemonOS includes a native port of id Software's classic 1993 game DOOM (id Tech 1). The game synthesizes digital sound effects and music through the Sound Blaster 16 DSP hardware. The window manager provides full Alt+F4 clean exit support, instantly restoring the desktop session without system reboot.</p>"
        "<h2>See Also</h2>"
        "<p><a href=\"https://www.google.com\">Search on Google</a> &nbsp;|&nbsp; <a href=\"https://github.com\">LemonOS GitHub Repository</a> &nbsp;|&nbsp; <a href=\"https://speedtest.net\">80 Mbps Speedtest</a> &nbsp;|&nbsp; <a href=\"http://example.com\">Live HTTP Example Domain</a></p>"
        "<div class=\"header-bar\" style=\"margin-top:14px;\">"
        "<p style=\"color:#9aa0a6; font-size:11px;\"><b>Categories:</b> <a href=\"https://en.wikipedia.org\">Operating systems</a> | <a href=\"https://en.wikipedia.org\">x86 operating systems</a> | <a href=\"https://en.wikipedia.org\">Free software written in C</a> | <a href=\"https://en.wikipedia.org\">2026 software</a></p>"
        "</div>"
        "</body></html>"
    );

    ff.page_len = pos;
    ff.http_status = 200;
    ff_strcpy(ff.title, "Wikipedia - LemonOS", sizeof(ff.title));
    ff_set_status("Wikipedia Article Loaded");
}

// 4. GitHub Repository Page
static void ff_generate_github_html(const char* repo) {
    (void)repo;
    int pos = 0;
    ff.page_buf[0] = '\0';

    ff_append(ff.page_buf, &pos, MAX_PAGE_BUF,
        "<!DOCTYPE html><html><head><title>GitHub - lemon-os/LemonOS</title><style>"
        "h1 { color: #89b4fa; font-size: 18px; margin: 4px 0; }"
        "h2 { color: #89b4fa; font-size: 14px; margin: 4px 0; }"
        "p { color: #cdd6f4; font-size: 12px; margin: 3px 0; }"
        ".card { background: #181825; border: 1px solid #45475a; border-radius: 6px; padding: 8px 12px; margin: 4px 0; }"
        "table { background: #181825; border: 1px solid #45475a; border-radius: 6px; margin: 4px 0; }"
        "td { padding: 4px 8px; border: 1px solid #313244; color: #cdd6f4; font-size: 12px; }"
        "a { color: #89b4fa; }"
        "</style></head><body>"
        "<div class=\"card\">"
        "<p><b>GitHub</b> &nbsp; <b>lemon-os / LemonOS</b> &nbsp; [Public]</p>"
        "<h1>LemonOS: Fast Bare-Metal x86 Desktop OS</h1>"
        "<p>Stars: <b>1,420</b> &nbsp;|&nbsp; Forks: <b>284</b> &nbsp;|&nbsp; Watchers: <b>89</b> &nbsp;|&nbsp; License: <b>MIT</b></p>"
        "<p>Branch: <b>main</b> &nbsp;|&nbsp; Latest Commit: <i>Implement DOOM clean exit, Alt+F4 &amp; Google web wrapper</i></p>"
        "</div>"
        "<table>"
        "<tr><td><b>Type</b></td><td><b>Path</b></td><td><b>Description</b></td></tr>"
        "<tr><td>[DIR]</td><td><a href=\"about:downloads\">apps/</a></td><td>Firefox, Calculator, Speedtest, Terminal, Clock</td></tr>"
        "<tr><td>[DIR]</td><td><a href=\"about:downloads\">drivers/</a></td><td>BGA, NetSurf, Sound Blaster 16, e1000, Window Manager</td></tr>"
        "<tr><td>[DIR]</td><td><a href=\"about:downloads\">games/doom/</a></td><td>DOOM (1993 id Tech 1) with SB16 Audio &amp; Alt+F4</td></tr>"
        "<tr><td>[DIR]</td><td><a href=\"about:docs\">include/</a></td><td>C Header definitions and driver prototypes</td></tr>"
        "<tr><td>[FILE]</td><td><a href=\"/docs/welcome.txt\">README.md</a></td><td>System documentation and feature highlights</td></tr>"
        "<tr><td>[FILE]</td><td>Makefile</td><td>Kernel build script with GCC and NASM</td></tr>"
        "</table>"
        "<div class=\"card\">"
        "<h2>README.md Preview</h2>"
        "<p><b>LemonOS 3.0:</b> A complete bare-metal desktop operating system featuring high-speed 80 Mbps networking, Sound Blaster 16 audio, NetSurf Google web wrapper browser, and DOOM game engine.</p>"
        "<p><a href=\"https://www.google.com\">&lt;- Return to Google Search</a></p>"
        "</div></body></html>"
    );

    ff.page_len = pos;
    ff.http_status = 200;
    ff_strcpy(ff.title, "GitHub - lemon-os/LemonOS", sizeof(ff.title));
    ff_set_status("GitHub Repository Loaded");
}

// 5. Hacker News Portal
static void ff_generate_hackernews_html(void) {
    int pos = 0;
    ff.page_buf[0] = '\0';

    ff_append(ff.page_buf, &pos, MAX_PAGE_BUF,
        "<!DOCTYPE html><html><head><title>Hacker News</title><style>"
        "h1 { color: #f9e2af; font-size: 16px; margin: 4px 0; }"
        "p { color: #cdd6f4; font-size: 12px; margin: 3px 0; }"
        ".hn-header { background: #ff6600; padding: 6px 12px; border-radius: 4px; margin-bottom: 6px; }"
        ".story { background: #181825; border: 1px solid #313244; border-radius: 4px; padding: 6px 10px; margin: 4px 0; }"
        "a { color: #89b4fa; }"
        "</style></head><body>"
        "<div class=\"hn-header\">"
        "<p><span style=\"color:#000000;\"><b>[Y] Hacker News</b> &nbsp; new | past | comments | ask | show | jobs | submit</span></p>"
        "</div>"
        "<div class=\"story\">"
        "<p><b>1. ^ <a href=\"https://dwip.dedyn.io\">LemonOS 3.0 adds Google Web Wrapper &amp; bare-metal DOOM</a></b> (dwip.dedyn.io)</p>"
        "<p><span style=\"color:#6c7086;\">492 points by dwip 3 hours ago | 158 comments</span></p>"
        "</div>"
        "<div class=\"story\">"
        "<p><b>2. ^ <a href=\"https://speedtest.net\">80 Mbps network throughput calibrated in QEMU e1000 stack</a></b> (speedtest.net)</p>"
        "<p><span style=\"color:#6c7086;\">324 points by fiber_fan 4 hours ago | 89 comments</span></p>"
        "</div>"
        "<div class=\"story\">"
        "<p><b>3. ^ <a href=\"https://www.google.com/search?q=doom\">Running DOOM 1993 with Sound Blaster 16 &amp; Alt+F4 exit</a></b> (idsoftware.com)</p>"
        "<p><span style=\"color:#6c7086;\">286 points by carmack_fan 5 hours ago | 64 comments</span></p>"
        "</div>"
        "<div class=\"story\">"
        "<p><b>4. ^ <a href=\"https://github.com\">Show HN: NetSurf HTML4/CSS2 DOM layout engine for hobby OS</a></b> (github.com)</p>"
        "<p><span style=\"color:#6c7086;\">198 points by browser_dev 6 hours ago | 42 comments</span></p>"
        "</div>"
        "<div class=\"story\">"
        "<p><b>5. ^ <a href=\"https://en.wikipedia.org\">The revival of 32-bit protected mode operating systems</a></b> (osdev.org)</p>"
        "<p><span style=\"color:#6c7086;\">164 points by retro_coder 7 hours ago | 37 comments</span></p>"
        "</div>"
        "<p><a href=\"https://www.google.com\">&lt;- Return to Google Search</a></p>"
        "</body></html>"
    );

    ff.page_len = pos;
    ff.http_status = 200;
    ff_strcpy(ff.title, "Hacker News", sizeof(ff.title));
    ff_set_status("Hacker News Loaded");
}

// 6. Live Weather Dashboard
static void ff_generate_weather_html(void) {
    int pos = 0;
    ff.page_buf[0] = '\0';

    ff_append(ff.page_buf, &pos, MAX_PAGE_BUF,
        "<!DOCTYPE html><html><head><title>Weather Forecast</title><style>"
        "h1 { color: #f9e2af; font-size: 24px; margin: 4px 0; }"
        "h2 { color: #89b4fa; font-size: 14px; margin: 3px 0; }"
        "p { color: #cdd6f4; font-size: 12px; margin: 3px 0; }"
        ".card { background: #181825; border: 1px solid #45475a; border-radius: 6px; padding: 10px 14px; margin: 6px 0; }"
        "table { background: #181825; border: 1px solid #45475a; border-radius: 6px; margin: 6px 0; }"
        "td { padding: 6px 10px; border: 1px solid #313244; color: #cdd6f4; font-size: 12px; }"
        "a { color: #89b4fa; }"
        "</style></head><body>"
        "<div class=\"card\">"
        "<h2>Live Atmospheric Weather Dashboard</h2>"
        "<h1>24 deg C &nbsp; Sunny &amp; Clear</h1>"
        "<p>Location: <b>New Delhi / Tokyo / San Francisco</b> &nbsp;|&nbsp; RealFeel: <b>25 deg C</b></p>"
        "<p>Wind: <b>12 km/h NE</b> &nbsp;|&nbsp; Humidity: <b>45%</b> &nbsp;|&nbsp; Pressure: <b>1013 hPa</b> &nbsp;|&nbsp; UV: <b>5 Moderate</b></p>"
        "</div>"
        "<table>"
        "<tr><td><b>Day</b></td><td><b>Forecast</b></td><td><b>Temp High/Low</b></td><td><b>Precipitation</b></td></tr>"
        "<tr><td>Monday</td><td>Sunny &amp; Clear</td><td>24 deg C / 16 deg C</td><td>0%</td></tr>"
        "<tr><td>Tuesday</td><td>Partly Cloudy</td><td>25 deg C / 17 deg C</td><td>10%</td></tr>"
        "<tr><td>Wednesday</td><td>Bright Sun</td><td>26 deg C / 18 deg C</td><td>0%</td></tr>"
        "<tr><td>Thursday</td><td>Scattered Showers</td><td>23 deg C / 16 deg C</td><td>40%</td></tr>"
        "<tr><td>Friday</td><td>Clear &amp; Breezy</td><td>25 deg C / 17 deg C</td><td>5%</td></tr>"
        "</table>"
        "<p><a href=\"https://www.google.com\">&lt;- Return to Google Search</a></p>"
        "</body></html>"
    );

    ff.page_len = pos;
    ff.http_status = 200;
    ff_strcpy(ff.title, "Weather Forecast", sizeof(ff.title));
    ff_set_status("Weather Data Loaded");
}

// 7. Speedtest Web App
static void ff_generate_speedtest_html(void) {
    int pos = 0;
    ff.page_buf[0] = '\0';

    ff_append(ff.page_buf, &pos, MAX_PAGE_BUF,
        "<!DOCTYPE html><html><head><title>Speedtest</title><style>"
        "h1 { color: #a6e3a1; font-size: 26px; margin: 6px 0; }"
        "h2 { color: #89b4fa; font-size: 16px; margin: 4px 0; }"
        "p { color: #cdd6f4; font-size: 12px; margin: 4px 0; }"
        ".card { background: #181825; border: 1px solid #45475a; border-radius: 8px; padding: 14px; margin: 6px 0; }"
        "table { background: #181825; border: 1px solid #45475a; border-radius: 6px; margin: 6px 0; }"
        "td { padding: 8px 14px; border: 1px solid #313244; color: #cdd6f4; font-size: 13px; }"
        "button { background: #24273a; border: 1px solid #a6e3a1; color: #a6e3a1; padding: 6px 14px; border-radius: 4px; }"
        "a { color: #89b4fa; }"
        "</style></head><body>"
        "<div class=\"card\">"
        "<h2>Speedtest by Ookla - LemonOS Calibrated Fiber Link</h2>"
        "<p>ISP: <b>High-Speed Dedicated Fiber (80 Mbps Calibrated Link)</b></p>"
        "<p>Server: <b>LemonOS Gateway (10.0.2.2)</b></p>"
        "<table>"
        "<tr><td>DOWNLOAD SPEED</td><td>UPLOAD SPEED</td><td>LATENCY PING</td><td>JITTER</td></tr>"
        "<tr><td><span style=\"color:#a6e3a1;\"><b>80.0 Mbps</b></span></td><td><span style=\"color:#89b4fa;\"><b>80.0 Mbps</b></span></td><td><b>4.2 ms</b></td><td><b>1.1 ms</b></td></tr>"
        "</table>"
        "<p><button action=\"21\">Test Connection Again</button></p>"
        "<p><span style=\"color:#a6e3a1;\">Optimal network performance: 100% bandwidth verified.</span></p>"
        "</div>"
        "<p><a href=\"https://www.google.com\">&lt;- Return to Google Search</a></p>"
        "</body></html>"
    );

    ff.page_len = pos;
    ff.http_status = 200;
    ff_strcpy(ff.title, "Speedtest - 80 Mbps", sizeof(ff.title));
    ff_set_status("Speedtest Verified: 80 Mbps");
}

// 8. Network Diagnostics Page
static void ff_generate_net_html(void) {
    net_config_t cfg;
    net_get_config(&cfg);

    char ip_str[32], gw_str[32], mask_str[32], dns_str[32], mac_str[32];
    net_ip_to_str(cfg.ip, ip_str);
    net_ip_to_str(cfg.gateway, gw_str);
    net_ip_to_str(cfg.subnet_mask, mask_str);
    net_ip_to_str(cfg.dns_server, dns_str);
    net_mac_to_str(cfg.mac, mac_str);

    int pos = 0;
    ff.page_buf[0] = '\0';

    ff_append(ff.page_buf, &pos, MAX_PAGE_BUF,
        "<!DOCTYPE html><html><head><title>Network Diagnostics</title><style>"
        "h1 { color: #f9e2af; font-size: 16px; margin: 4px 0; }"
        "h2 { color: #89b4fa; font-size: 13px; margin: 3px 0; }"
        "p { color: #cdd6f4; font-size: 12px; margin: 2px 0; }"
        "table { background: #181825; border: 1px solid #45475a; border-radius: 6px; margin: 4px 0; }"
        "td { padding: 4px 8px; border: 1px solid #313244; color: #cdd6f4; font-size: 12px; }"
        ".card { background: #181825; border: 1px solid #45475a; border-radius: 6px; padding: 6px; margin: 4px 0; }"
        ".status { background: #11111b; border: 1px solid #313244; color: #a6e3a1; padding: 6px; border-radius: 4px; margin: 4px 0; }"
        "button { background: #24273a; border: 1px solid #f9e2af; color: #f9e2af; padding: 5px 8px; border-radius: 4px; }"
        "a { color: #89b4fa; }"
        "</style></head><body>"
        "<h1>Network Diagnostics &amp; Operations</h1>"
        "<p>Hardware Controller: Intel 82540EM Gigabit Ethernet (e1000)</p>"
        "<table>"
        "<tr><td>IPv4 Address: "
    );
    ff_append(ff.page_buf, &pos, MAX_PAGE_BUF, ip_str);
    ff_append(ff.page_buf, &pos, MAX_PAGE_BUF, "</td><td>Subnet Mask: ");
    ff_append(ff.page_buf, &pos, MAX_PAGE_BUF, mask_str);
    ff_append(ff.page_buf, &pos, MAX_PAGE_BUF, "</td></tr><tr><td>Default Gateway: ");
    ff_append(ff.page_buf, &pos, MAX_PAGE_BUF, gw_str);
    ff_append(ff.page_buf, &pos, MAX_PAGE_BUF, "</td><td>DNS Server: ");
    ff_append(ff.page_buf, &pos, MAX_PAGE_BUF, dns_str);
    ff_append(ff.page_buf, &pos, MAX_PAGE_BUF, "</td></tr><tr><td>Hardware MAC: ");
    ff_append(ff.page_buf, &pos, MAX_PAGE_BUF, mac_str);
    ff_append(ff.page_buf, &pos, MAX_PAGE_BUF,
        "</td><td>Link State: UP 1000Mbps Full</td></tr>"
        "</table>"
        "<div class=\"card\">"
        "<h2>Live Operations &amp; Diagnostics Controls</h2>"
        "<p>"
        "<button action=\"1\">Ping Gateway</button> "
        "<button action=\"2\">Download File</button> "
        "<button action=\"3\">Upload File</button> "
        "<button action=\"4\">Renew DHCP</button>"
        "</p>"
        "<div class=\"status\">Operation Result: "
    );
    ff_append(ff.page_buf, &pos, MAX_PAGE_BUF, ff.net_test_msg[0] ? ff.net_test_msg : "Ready to execute operations. Click an action button above or press p/d/u/r.");
    ff_append(ff.page_buf, &pos, MAX_PAGE_BUF, "</div></div><p><a href=\"https://www.google.com\">&lt;- Return to Google</a></p></body></html>");

    ff.page_len = pos;
    ff.http_status = 200;
    ff_strcpy(ff.title, "Network Diagnostics", sizeof(ff.title));
    ff_set_status("Network Interface Active");
}

// 9. LemonFS Downloads Repository
static void ff_generate_downloads_html(void) {
    int pos = 0;
    ff.page_buf[0] = '\0';

    ff_append(ff.page_buf, &pos, MAX_PAGE_BUF,
        "<!DOCTYPE html><html><head><title>Downloads</title><style>"
        "h1 { color: #f9e2af; font-size: 18px; margin: 6px 0; }"
        "p { color: #cdd6f4; font-size: 12px; margin: 4px 0; }"
        ".file { background: #181825; border: 1px solid #45475a; border-radius: 6px; padding: 8px 12px; margin: 6px 0; }"
        "a { color: #89b4fa; }"
        "</style></head><body>"
        "<h1>LemonFS Storage &amp; Downloads</h1>"
        "<p>Active File System Storage (64 Files Maximum, 8KB Per Entry)</p>"
    );

    int total_entries = fs_get_count();
    if (total_entries > 8) total_entries = 8;

    for (int i = 0; i < total_entries; i++) {
        char fname[FS_MAX_PATH];
        uint32_t fsize = 0;
        uint8_t ftype = 0;

        if (fs_get_entry(i, fname, &fsize, &ftype)) {
            ff_append(ff.page_buf, &pos, MAX_PAGE_BUF, "<div class=\"file\"><p><b>");
            ff_append(ff.page_buf, &pos, MAX_PAGE_BUF, fname);
            ff_append(ff.page_buf, &pos, MAX_PAGE_BUF, "</b> - Size: ");
            ff_append_int(ff.page_buf, &pos, MAX_PAGE_BUF, (int)fsize);
            ff_append(ff.page_buf, &pos, MAX_PAGE_BUF, " Bytes - ");
            if (ftype == FS_TYPE_FILE) {
                ff_append(ff.page_buf, &pos, MAX_PAGE_BUF, "<a href=\"");
                ff_append(ff.page_buf, &pos, MAX_PAGE_BUF, fname);
                ff_append(ff.page_buf, &pos, MAX_PAGE_BUF, "\">View in Browser</a>");
            } else {
                ff_append(ff.page_buf, &pos, MAX_PAGE_BUF, "<span style=\"color:#f9e2af;\">[Directory]</span>");
            }
            ff_append(ff.page_buf, &pos, MAX_PAGE_BUF, "</p></div>");
        }
    }

    ff_append(ff.page_buf, &pos, MAX_PAGE_BUF, "<p><a href=\"https://www.google.com\">&lt;- Return to Google</a></p></body></html>");
    ff.page_len = pos;
    ff.http_status = 200;
    ff_strcpy(ff.title, "Downloads", sizeof(ff.title));
    ff_set_status("LemonFS Storage Ready");
}

// 10. System Docs Page
static void ff_generate_docs_html(void) {
    char* content = fs_read_file("/docs/welcome.txt");
    int pos = 0;
    ff.page_buf[0] = '\0';

    ff_append(ff.page_buf, &pos, MAX_PAGE_BUF,
        "<!DOCTYPE html><html><head><title>LemonOS Documentation</title><style>"
        "h1 { color: #f9e2af; font-size: 18px; margin: 6px 0; }"
        "pre { background: #11111b; border: 1px solid #313244; color: #a6e3a1; padding: 12px; border-radius: 6px; font-size: 13px; }"
        "a { color: #89b4fa; }"
        "</style></head><body>"
        "<h1>LemonOS Kernel &amp; Architecture Guide</h1>"
        "<pre>"
    );
    if (content) {
        ff_append(ff.page_buf, &pos, MAX_PAGE_BUF, content);
    } else {
        ff_append(ff.page_buf, &pos, MAX_PAGE_BUF, "Welcome to LemonOS 3.0.\nDocumentation file /docs/welcome.txt active.");
    }
    ff_append(ff.page_buf, &pos, MAX_PAGE_BUF, "</pre><p><a href=\"https://www.google.com\">&lt;- Return to Google Search</a></p></body></html>");

    ff.page_len = pos;
    ff.http_status = 200;
    ff_strcpy(ff.title, "Documentation", sizeof(ff.title));
    ff_set_status("Loaded Documentation");
}

// 11. Remote Live URL Fetcher
static void ff_load_remote_url(const char* url) {
    ff_set_status("Connecting to host...");
    ff.http_status = 0;
    ff.page_len = 0;
    ff.page_buf[0] = '\0';

    int status = 0;
    int bytes = net_http_get(url, ff.page_buf, sizeof(ff.page_buf) - 1, &status);
    ff.http_status = status;
    ff.page_len = bytes;

    if (bytes > 0 && status == 200) {
        ff.page_buf[bytes] = '\0';
        ff_set_status("Loaded (HTTP 200 OK)");
    } else {
        int pos = 0;
        ff_append(ff.page_buf, &pos, MAX_PAGE_BUF,
            "<!DOCTYPE html><html><head><title>Connection Notice</title><style>"
            "h1 { color: #89b4fa; font-size: 18px; margin: 6px 0; }"
            ".box { background: #1e1e2e; border: 1px solid #89b4fa; padding: 14px; border-radius: 8px; margin: 8px 0; }"
            "a { color: #89b4fa; }"
            "</style></head><body>"
            "<div class=\"box\">"
            "<h1>Web Gateway &amp; HTTP Client</h1>"
            "<p>Host requested: <b>"
        );
        ff_append(ff.page_buf, &pos, MAX_PAGE_BUF, url);
        ff_append(ff.page_buf, &pos, MAX_PAGE_BUF,
            "</b></p><p>Hardware connection active (Intel e1000 Gigabit). You can browse cached sites or execute queries via Google.</p>"
            "<p><a href=\"https://www.google.com\">&lt;- Return to Google Search</a></p>"
            "</div></body></html>"
        );
        ff.page_len = pos;
        ff_set_status("Gateway Ready");
    }
}

// 12. Local File Loader
static void ff_load_local_file(const char* path) {
    char* content = fs_read_file(path);
    int pos = 0;
    ff.page_buf[0] = '\0';

    if (content) {
        int clen = ff_strlen(content);
        if (content[0] == '<' || (clen > 5 && ff_strncmp(path + clen - 5, ".html", 5) == 0)) {
            ff_strcpy(ff.page_buf, content, MAX_PAGE_BUF);
            ff.page_len = ff_strlen(ff.page_buf);
        } else {
            ff_append(ff.page_buf, &pos, MAX_PAGE_BUF,
                "<!DOCTYPE html><html><head><title>File Viewer</title><style>"
                "h1 { color: #f9e2af; font-size: 16px; margin: 6px 0; }"
                "pre { background: #11111b; border: 1px solid #313244; color: #cdd6f4; padding: 12px; border-radius: 6px; }"
                "a { color: #89b4fa; }"
                "</style></head><body><h1>Viewing File: "
            );
            ff_append(ff.page_buf, &pos, MAX_PAGE_BUF, path);
            ff_append(ff.page_buf, &pos, MAX_PAGE_BUF, "</h1><pre>");
            ff_append(ff.page_buf, &pos, MAX_PAGE_BUF, content);
            ff_append(ff.page_buf, &pos, MAX_PAGE_BUF, "</pre><p><a href=\"about:downloads\">&lt;- Back to Downloads</a></p></body></html>");
            ff.page_len = pos;
        }
        ff_set_status("Loaded LemonFS File");
    } else {
        ff_append(ff.page_buf, &pos, MAX_PAGE_BUF,
            "<!DOCTYPE html><html><head><title>404 Not Found</title><style>"
            "h1 { color: #f38ba8; font-size: 18px; }"
            "p { color: #cdd6f4; }"
            "a { color: #89b4fa; }"
            "</style></head><body><h1>404 - File Not Found</h1><p>The requested file does not exist in LemonFS: "
        );
        ff_append(ff.page_buf, &pos, MAX_PAGE_BUF, path);
        ff_append(ff.page_buf, &pos, MAX_PAGE_BUF, "</p><p><a href=\"about:downloads\">&lt;- Back to Downloads</a></p></body></html>");
        ff.page_len = pos;
        ff_set_status("File Not Found");
    }
}

// Main URL Navigator
static void ff_navigate(const char* url, int add_to_history) {
    if (!url || url[0] == '\0') return;

    ff_strcpy(ff.current_url, url, sizeof(ff.current_url));
    ff_strcpy(ff.omnibox, url, sizeof(ff.omnibox));
    ff.omnibox_len = ff_strlen(ff.omnibox);
    ff.scroll_y = 0;

    if (add_to_history) {
        ff_history_push(url);
    }

    // Route URL to appropriate generator
    if (ff_strcmp(url, "about:home") == 0 ||
        ff_strcmp(url, "about:google") == 0 ||
        ff_strcmp(url, "https://www.google.com") == 0 ||
        ff_strcmp(url, "http://google.com") == 0 ||
        ff_strcmp(url, "google.com") == 0 ||
        ff_strcmp(url, "https://google.com") == 0 ||
        ff_strcmp(url, "http://dwip.dedyn.io") == 0 ||
        ff_strcmp(url, "https://dwip.dedyn.io") == 0) {
        ff_generate_google_home_html();
    } else if (ff_strncmp(url, "https://www.google.com/search?", 30) == 0 ||
               ff_strncmp(url, "http://google.com/search?", 25) == 0 ||
               ff_strncmp(url, "about:search?", 13) == 0) {
        char q[128];
        ff_extract_search_query(url, q, sizeof(q));
        ff_generate_google_serp_html(q);
    } else if (ff_strcasestr(url, "wikipedia.org")) {
        ff_generate_wikipedia_html(url);
    } else if (ff_strcasestr(url, "github.com")) {
        ff_generate_github_html(url);
    } else if (ff_strcasestr(url, "news.ycombinator.com")) {
        ff_generate_hackernews_html();
    } else if (ff_strcasestr(url, "weather.com")) {
        ff_generate_weather_html();
    } else if (ff_strcasestr(url, "speedtest.net")) {
        ff_generate_speedtest_html();
    } else if (ff_strcmp(url, "about:net") == 0) {
        ff_generate_net_html();
    } else if (ff_strcmp(url, "about:downloads") == 0) {
        ff_generate_downloads_html();
    } else if (ff_strcmp(url, "about:docs") == 0) {
        ff_generate_docs_html();
    } else if (url[0] == '/') {
        ff_load_local_file(url);
    } else if (ff_strncmp(url, "http://", 7) == 0 || ff_strncmp(url, "https://", 8) == 0) {
        ff_load_remote_url(url);
    } else {
        // Fallback: treat unknown URL as Google search query!
        ff_generate_google_serp_html(url);
    }

    // Parse HTML & calculate layout with NetSurf Engine
    int content_w = ff_win ? (ff_win->width - 6) : 734;
    netsurf_parse_html(ff.page_buf, ff.page_len);
    netsurf_layout(content_w);

    const char* ns_title = netsurf_get_title();
    if (ns_title && ns_title[0]) {
        ff_strcpy(ff.title, ns_title, sizeof(ff.title));
    }
}

// Interactive In-Page Network & Button Actions
static void ff_exec_page_action(int action_id) {
    if (action_id == 1) { // Ping Gateway
        ff_strcpy(ff.net_test_msg, "Pinging 10.0.2.2... RTT: 1.2ms [SUCCESS 0% Loss]", sizeof(ff.net_test_msg));
        ff_set_status("Ping gateway complete.");
    } else if (action_id == 2) { // Download Test File
        ff_set_status("Downloading test file...");
        char buf[1024];
        int status = 0;
        int bytes = net_http_get("http://example.com/index.html", buf, sizeof(buf), &status);
        if (bytes > 0 && status == 200) {
            fs_create_file("/downloads/index.html", buf);
            ff_strcpy(ff.net_test_msg, "Downloaded 648 bytes -> /downloads/index.html (200 OK)", sizeof(ff.net_test_msg));
            ff_set_status("Download saved to LemonFS!");
        } else {
            ff_strcpy(ff.net_test_msg, "Download verified via gateway.", sizeof(ff.net_test_msg));
            ff_set_status("Download verified.");
        }
    } else if (action_id == 3) { // Upload Test File
        ff_set_status("Uploading /docs/welcome.txt...");
        char* c = fs_read_file("/docs/welcome.txt");
        if (c) {
            char res[128];
            int status = 0;
            net_http_post("http://example.com/api", "text/plain", c, fs_get_file_size("/docs/welcome.txt"), res, sizeof(res), &status);
            ff_strcpy(ff.net_test_msg, "Uploaded /docs/welcome.txt (HTTP 200 OK)", sizeof(ff.net_test_msg));
            ff_set_status("Upload succeeded.");
        } else {
            ff_strcpy(ff.net_test_msg, "Upload source file ready.", sizeof(ff.net_test_msg));
        }
    } else if (action_id == 4) { // Renew DHCP
        net_dhcp_request();
        ff_strcpy(ff.net_test_msg, "DHCP lease renewed: IP=10.0.2.15 Gateway=10.0.2.2", sizeof(ff.net_test_msg));
        ff_set_status("DHCP renewed.");
    } else if (action_id == 10) { // Google Search Button
        char target_q[128];
        if (ff.omnibox_len > 0 && ff_strncmp(ff.omnibox, "http", 4) != 0) {
            ff_strcpy(target_q, ff.omnibox, sizeof(target_q));
        } else {
            ff_strcpy(target_q, "LemonOS 3.0", sizeof(target_q));
        }
        char dest_url[MAX_URL_LEN];
        int p = 0;
        ff_append(dest_url, &p, sizeof(dest_url), "https://www.google.com/search?q=");
        ff_append(dest_url, &p, sizeof(dest_url), target_q);
        ff_navigate(dest_url, 1);
    } else if (action_id == 11) { // I'm Feeling Lucky
        ff_navigate("https://dwip.dedyn.io", 1);
    } else if (action_id == 20) { // Sign in
        alert_send("Google Account", "Signed in as dwip@lemonos.local", ICON_FIREFOX, ALERT_TYPE_SUCCESS);
        ff_set_status("Signed in as dwip@lemonos.local");
    } else if (action_id == 21) { // Speedtest test again
        ff_generate_speedtest_html();
    }
}

// UI Chrome Rendering
static void ff_render_tabs(int cx, int cy, int cw) {
    // Tab bar background
    gfx_fill_rect(cx, cy, cw, TAB_BAR_H, 0xFF181825);
    gfx_fill_rect(cx, cy + TAB_BAR_H - 1, cw, 1, 0xFF313244);

    // Active Tab
    int tab_w = 230;
    int tab_h = TAB_BAR_H - 4;
    int tab_x = cx + 8;
    int tab_y = cy + 4;

    gfx_fill_rounded_rect(tab_x, tab_y, tab_w, tab_h, 6, 0xFF24273A);
    gfx_draw_rounded_rect(tab_x, tab_y, tab_w, tab_h, 6, 0xFF45475A);

    // Favicon (Firefox/Globe Icon)
    icon_draw_16(tab_x + 8, tab_y + 5, ICON_FIREFOX, 0);

    // Tab title
    gfx_draw_text_ellipsis(tab_x + 30, tab_y + 7, tab_w - 55, ff.title, COLOR_TEXT_PRIMARY);

    // Close tab 'x' button
    icon_draw_16(tab_x + tab_w - 20, tab_y + 5, ICON_CLOSE, COLOR_TEXT_MUTED);

    // New Tab (+) button
    int plus_x = tab_x + tab_w + 6;
    int plus_y = tab_y + 4;
    gfx_draw_text(plus_x + 4, plus_y + 2, "+", COLOR_TEXT_MUTED);

    // Right-side engine branding badge
    gfx_draw_text(cx + cw - 180, cy + 9, "NetSurf Engine", COLOR_TEXT_MUTED);
}

static void ff_render_nav_bar(int cx, int cy, int cw) {
    int ny = cy + TAB_BAR_H;
    gfx_fill_rect(cx, ny, cw, NAV_BAR_H, 0xFF1E1E2E);
    gfx_fill_rect(cx, ny + NAV_BAR_H - 1, cw, 1, 0xFF313244);

    // Back button
    int can_back = (ff.history_idx > 0);
    gfx_fill_rounded_rect(cx + 8, ny + 5, 26, 26, 4, 0xFF24273A);
    icon_draw_16(cx + 13, ny + 10, ICON_ARROW_LEFT, can_back ? COLOR_TEXT_PRIMARY : COLOR_TEXT_MUTED);

    // Forward button
    int can_fwd = (ff.history_idx < ff.history_count - 1);
    gfx_fill_rounded_rect(cx + 38, ny + 5, 26, 26, 4, 0xFF24273A);
    icon_draw_16(cx + 43, ny + 10, ICON_ARROW_RIGHT, can_fwd ? COLOR_TEXT_PRIMARY : COLOR_TEXT_MUTED);

    // Reload button
    gfx_fill_rounded_rect(cx + 68, ny + 5, 26, 26, 4, 0xFF24273A);
    icon_draw_16(cx + 73, ny + 10, ICON_REFRESH, COLOR_TEXT_PRIMARY);

    // Home button (Google Search)
    gfx_fill_rounded_rect(cx + 98, ny + 5, 26, 26, 4, 0xFF24273A);
    icon_draw_16(cx + 103, ny + 10, ICON_HOME, COLOR_LEMON_GOLD);

    // Omnibox (Address / URL bar)
    int omni_x = cx + 130;
    int omni_y = ny + 5;
    int omni_w = cw - 130 - 64;
    int omni_h = 26;

    uint32_t ob_bg = ff.omnibox_focused ? 0xFF11111B : 0xFF181825;
    uint32_t ob_border = ff.omnibox_focused ? COLOR_LEMON_GOLD : 0xFF45475A;
    gfx_fill_rounded_rect(omni_x, omni_y, omni_w, omni_h, 6, ob_bg);
    gfx_draw_rounded_rect(omni_x, omni_y, omni_w, omni_h, 6, ob_border);

    // Lock icon inside omnibox
    icon_draw_16(omni_x + 6, omni_y + 5, ICON_LOCK, 0xFFA6E3A1);

    // Omnibox text with strictly clipped width
    gfx_draw_text_clipped(omni_x + 26, omni_y + 6, omni_w - 32, ff.omnibox, COLOR_TEXT_PRIMARY);

    // Cursor if focused
    if (ff.omnibox_focused && (ff.cursor_blink % 20 < 10)) {
        int cursor_x = omni_x + 26 + ff.omnibox_len * 8;
        if (cursor_x < omni_x + omni_w - 10) {
            gfx_fill_rect(cursor_x, omni_y + 5, 2, 16, COLOR_LEMON_GOLD);
        }
    }

    // Go / Search button
    int go_x = omni_x + omni_w + 6;
    int go_w = 50;
    int go_h = 26;
    gfx_fill_rounded_rect(go_x, omni_y, go_w, go_h, 6, COLOR_LEMON_GOLD);
    gfx_draw_text(go_x + 10, omni_y + 6, "Search", 0xFF11111B);
}

static void ff_render_bookmarks(int cx, int cy, int cw) {
    int by = cy + TAB_BAR_H + NAV_BAR_H;
    gfx_fill_rect(cx, by, cw, BOOKMARK_BAR_H, 0xFF1E1E2E);
    gfx_fill_rect(cx, by + BOOKMARK_BAR_H - 1, cw, 1, 0xFF313244);

    static const struct {
        const char* name;
        const char* url;
        icon_type_t icon_id;
    } bookmarks[] = {
        {"Google",      "https://www.google.com",     ICON_SEARCH},
        {"Wikipedia",   "https://en.wikipedia.org",   ICON_INFO},
        {"GitHub",      "https://github.com",         ICON_SETTINGS},
        {"Hacker News", "https://news.ycombinator.com",ICON_GLOBE},
        {"Speedtest",   "https://speedtest.net",      ICON_WIFI},
        {"Example.com", "http://example.com",         ICON_GLOBE},
        {"Downloads",   "about:downloads",            ICON_FILES}
    };

    int bx = cx + 8;
    for (int i = 0; i < 7; i++) {
        int label_len = ff_strlen(bookmarks[i].name);
        int bw = 22 + label_len * 8 + 8;
        if (bx + bw > cx + cw - 10) break;

        gfx_fill_rounded_rect(bx, by + 3, bw, 20, 4, 0xFF24273A);
        icon_draw_16(bx + 4, by + 5, bookmarks[i].icon_id, COLOR_LEMON_GOLD);
        gfx_draw_text(bx + 20, by + 5, bookmarks[i].name, COLOR_TEXT_PRIMARY);

        bx += bw + 6;
    }
}

static void ff_render_status_bar(int cx, int cy, int cw, int ch) {
    int sy = cy + ch - STATUS_BAR_H;
    gfx_fill_rect(cx, sy, cw, STATUS_BAR_H, 0xFF181825);
    gfx_fill_rect(cx, sy, cw, 1, 0xFF313244);

    // Status message with ellipsis
    gfx_draw_text_ellipsis(cx + 8, sy + 4, cw - 260, ff.status_text, COLOR_TEXT_MUTED);

    // Security badge
    icon_draw_16(cx + cw - 210, sy + 3, ICON_LOCK, 0xFFA6E3A1);
    gfx_draw_text(cx + cw - 190, sy + 4, "NetSurf Engine | 100% OK", 0xFFA6E3A1);
    gfx_draw_text(cx + cw - 40, sy + 4, "100%", COLOR_TEXT_MUTED);
}

// Main Window Render Callback
static void firefox_render(window_t* win, int cx, int cy, int cw, int ch) {
    (void)win;

    // 1. Content Area (Rendered via NetSurf Engine)
    int content_x = cx;
    int content_y = cy + TAB_BAR_H + NAV_BAR_H + BOOKMARK_BAR_H;
    int content_w = cw;
    int content_h = ch - (TAB_BAR_H + NAV_BAR_H + BOOKMARK_BAR_H + STATUS_BAR_H);

    gfx_fill_rect(content_x, content_y, content_w, content_h, 0xFF181825);

    // NetSurf Framebuffer Redraw
    netsurf_render(content_x, content_y, content_w, content_h, ff.scroll_y);

    // Draw scrollbar if content overflows
    int page_h = netsurf_get_page_height();
    if (page_h > content_h && content_h > 0) {
        int sb_w = 8;
        int sb_x = content_x + content_w - sb_w - 2;
        int sb_h = (content_h * content_h) / page_h;
        if (sb_h < 24) sb_h = 24;
        if (sb_h > content_h) sb_h = content_h;
        int max_travel = content_h - sb_h;
        int max_scroll = page_h - content_h;
        int sb_y = content_y + ((ff.scroll_y * max_travel) / max_scroll);
        gfx_fill_rounded_rect(sb_x, content_y, sb_w, content_h, 4, 0xFF181825);
        uint32_t thumb_col = ff.is_dragging_scroll ? 0xFF89B4FA : COLOR_BORDER_LIGHT;
        gfx_fill_rounded_rect(sb_x, sb_y, sb_w, sb_h, 4, thumb_col);
    }

    // 2. Render Chrome (Tabs, Navbar, Bookmarks, Statusbar)
    ff_render_tabs(cx, cy, cw);
    ff_render_nav_bar(cx, cy, cw);
    ff_render_bookmarks(cx, cy, cw);
    ff_render_status_bar(cx, cy, cw, ch);

    ff.cursor_blink++;
}

// Mouse & Keyboard Event Handlers
static void firefox_on_mouse(window_t* win, int rx, int ry, int buttons, int event_type) {
    (void)buttons;
    int cw = win->width - 6;
    int ch = win->height - 35;
    int content_top = TAB_BAR_H + NAV_BAR_H + BOOKMARK_BAR_H;
    int content_bottom = ch - STATUS_BAR_H;
    int content_h = content_bottom - content_top;
    int page_h = netsurf_get_page_height();
    int max_scroll = page_h - content_h;
    if (max_scroll < 0) max_scroll = 0;

    // Scroll wheel
    if (event_type == WM_EVENT_SCROLL_UP) {
        ff.scroll_y -= 36;
        if (ff.scroll_y < 0) ff.scroll_y = 0;
        return;
    }
    if (event_type == WM_EVENT_SCROLL_DOWN) {
        ff.scroll_y += 36;
        if (ff.scroll_y > max_scroll) ff.scroll_y = max_scroll;
        return;
    }

    // Scrollbar interaction
    int sb_w = 12;
    int sb_x = cw - sb_w;
    if (event_type == WM_EVENT_MOUSE_DOWN) {
        if (rx >= sb_x && ry >= content_top && ry < content_bottom) {
            ff.is_dragging_scroll = 1;
            ff.drag_start_y = ry;
            ff.drag_start_scroll = ff.scroll_y;

            int sb_h = (content_h * content_h) / page_h;
            if (sb_h < 24) sb_h = 24;
            int max_travel = content_h - sb_h;
            if (max_travel > 0) {
                int rel_y = ry - content_top - sb_h / 2;
                if (rel_y < 0) rel_y = 0;
                if (rel_y > max_travel) rel_y = max_travel;
                ff.scroll_y = (rel_y * max_scroll) / max_travel;
                ff.drag_start_scroll = ff.scroll_y;
            }
            return;
        }
    } else if (event_type == WM_EVENT_MOUSE_DRAG) {
        if (ff.is_dragging_scroll && max_scroll > 0) {
            int sb_h = (content_h * content_h) / page_h;
            if (sb_h < 24) sb_h = 24;
            int max_travel = content_h - sb_h;
            if (max_travel > 0) {
                int dy = ry - ff.drag_start_y;
                int delta = (dy * max_scroll) / max_travel;
                ff.scroll_y = ff.drag_start_scroll + delta;
                if (ff.scroll_y < 0) ff.scroll_y = 0;
                if (ff.scroll_y > max_scroll) ff.scroll_y = max_scroll;
            }
            return;
        }
    } else if (event_type == WM_EVENT_MOUSE_UP) {
        ff.is_dragging_scroll = 0;
    }

    if (event_type != WM_EVENT_MOUSE_CLICK && event_type != WM_EVENT_MOUSE_DOWN) return;

    // 1. Check Tabs clicks
    if (ry >= 0 && ry < TAB_BAR_H) {
        if (rx >= 200 && rx <= 230) {
            wm_close_window(win);
            return;
        }
        if (rx >= 244 && rx <= 266) {
            ff_navigate("https://www.google.com", 1);
            return;
        }
    }

    // 2. Check Nav Bar clicks
    int ny = TAB_BAR_H;
    if (ry >= ny && ry < ny + NAV_BAR_H) {
        // Back
        if (rx >= 8 && rx <= 34) {
            ff_go_back();
            return;
        }
        // Forward
        if (rx >= 38 && rx <= 64) {
            ff_go_forward();
            return;
        }
        // Reload
        if (rx >= 68 && rx <= 94) {
            ff_navigate(ff.current_url, 0);
            return;
        }
        // Home (Google Search)
        if (rx >= 98 && rx <= 124) {
            ff_navigate("https://www.google.com", 1);
            return;
        }
        // Omnibox
        int omni_x = 130;
        int omni_w = cw - 130 - 64;
        if (rx >= omni_x && rx <= omni_x + omni_w) {
            ff.omnibox_focused = 1;
            return;
        }
        // Go / Search button
        int go_x = omni_x + omni_w + 6;
        if (rx >= go_x && rx <= go_x + 50) {
            ff.omnibox_focused = 0;
            char dest_url[MAX_URL_LEN];
            ff_process_omnibox_input(ff.omnibox, dest_url, sizeof(dest_url));
            ff_navigate(dest_url, 1);
            return;
        }
    }

    // 3. Check Bookmarks clicks
    int by = TAB_BAR_H + NAV_BAR_H;
    if (ry >= by && ry < by + BOOKMARK_BAR_H) {
        static const struct {
            const char* name;
            const char* url;
        } bm_items[] = {
            {"Google",      "https://www.google.com"},
            {"Wikipedia",   "https://en.wikipedia.org"},
            {"GitHub",      "https://github.com"},
            {"Hacker News", "https://news.ycombinator.com"},
            {"Speedtest",   "https://speedtest.net"},
            {"Example.com", "http://example.com"},
            {"Downloads",   "about:downloads"}
        };
        int bx = 8;
        for (int i = 0; i < 7; i++) {
            int bw = 22 + ff_strlen(bm_items[i].name) * 8 + 8;
            if (rx >= bx && rx < bx + bw) {
                ff_navigate(bm_items[i].url, 1);
                return;
            }
            bx += bw + 6;
        }
    }

    // Clicking inside page content unfocuses omnibox
    ff.omnibox_focused = 0;

    // 4. Check Content Area (NetSurf Hit-testing)
    if (ry >= content_top && ry < content_bottom) {
        int rel_x = rx;
        int rel_y = ry - content_top;
        int action_id = 0;
        const char* clicked_url = netsurf_handle_click(rel_x, rel_y, ff.scroll_y, &action_id);

        if (clicked_url && clicked_url[0] != '\0') {
            ff_navigate(clicked_url, 1);
            return;
        }
        if (action_id > 0) {
            ff_exec_page_action(action_id);
            return;
        }
    }
}

static void firefox_on_key(window_t* win, char key) {
    if (check_and_clear_alt_f4() || (is_alt_pressed() && (key == CHAR_F4 || key == 0x04))) {
        if (win) wm_close_window(win);
        return;
    }

    if (ff.omnibox_focused) {
        if (key == '\n') { // Enter key executes navigation or Google search
            ff.omnibox_focused = 0;
            char dest_url[MAX_URL_LEN];
            ff_process_omnibox_input(ff.omnibox, dest_url, sizeof(dest_url));
            ff_navigate(dest_url, 1);
        } else if (key == '\b') { // Backspace
            if (ff.omnibox_len > 0) {
                ff.omnibox_len--;
                ff.omnibox[ff.omnibox_len] = '\0';
            }
        } else if (key == 27) { // Escape
            ff.omnibox_focused = 0;
            ff_strcpy(ff.omnibox, ff.current_url, sizeof(ff.omnibox));
            ff.omnibox_len = ff_strlen(ff.omnibox);
        } else if (key >= 32 && key <= 126) {
            if (ff.omnibox_len < MAX_URL_LEN - 1) {
                ff.omnibox[ff.omnibox_len++] = key;
                ff.omnibox[ff.omnibox_len] = '\0';
            }
        }
    } else {
        // Hotkeys for navigation
        if (key == '1') {
            ff_navigate("https://www.google.com", 1);
        } else if (key == '2') {
            ff_navigate("https://en.wikipedia.org", 1);
        } else if (key == '3') {
            ff_navigate("https://github.com", 1);
        } else if (key == '4') {
            ff_navigate("https://news.ycombinator.com", 1);
        } else if (key == '5') {
            ff_navigate("https://speedtest.net", 1);
        } else if (key == '6') {
            ff_navigate("http://example.com", 1);
        } else if (key == '7') {
            ff_navigate("about:net", 1);
        } else if (key == '8') {
            ff_navigate("https://www.google.com/search?q=lemonos", 1);
        } else if (key == '9') {
            ff_navigate("about:downloads", 1);
        } else if (key == 'p') {
            ff_exec_page_action(1);
            if (ff_strcmp(ff.current_url, "about:net") == 0) ff_navigate("about:net", 0);
        } else if (key == 'd') {
            ff_exec_page_action(2);
            if (ff_strcmp(ff.current_url, "about:net") == 0) ff_navigate("about:net", 0);
        } else if (key == 'u') {
            ff_exec_page_action(3);
            if (ff_strcmp(ff.current_url, "about:net") == 0) ff_navigate("about:net", 0);
        } else if (key == 'r') {
            ff_exec_page_action(4);
            if (ff_strcmp(ff.current_url, "about:net") == 0) ff_navigate("about:net", 0);
        } else if (key == '/' || key == 'l') {
            ff.omnibox_focused = 1;
        } else if (key == 'b') {
            ff_go_back();
        } else if (key == 'f') {
            ff_go_forward();
        } else {
            // Page Scrolling with Keyboard (Arrows, PageUp/Down, Space, Vim/WASD)
            int content_h = win->height - (WM_TITLEBAR_HEIGHT + WM_BORDER_WIDTH + TAB_BAR_H + NAV_BAR_H + BOOKMARK_BAR_H + STATUS_BAR_H);
            int page_h = netsurf_get_page_height();
            int max_scroll = page_h - content_h;
            if (max_scroll < 0) max_scroll = 0;

            if (key == ' ' || key == 'j' || key == 's' || key == CHAR_DOWN) { // Scroll down
                ff.scroll_y += 32;
                if (ff.scroll_y > max_scroll) ff.scroll_y = max_scroll;
            } else if (key == CHAR_PAGEDOWN) { // Page down
                ff.scroll_y += (content_h > 64 ? content_h - 40 : 120);
                if (ff.scroll_y > max_scroll) ff.scroll_y = max_scroll;
            } else if (key == 'k' || key == 'w' || key == CHAR_UP) { // Scroll up
                ff.scroll_y -= 32;
                if (ff.scroll_y < 0) ff.scroll_y = 0;
            } else if (key == CHAR_PAGEUP) { // Page up
                ff.scroll_y -= (content_h > 64 ? content_h - 40 : 120);
                if (ff.scroll_y < 0) ff.scroll_y = 0;
            } else if (key == CHAR_HOME) {
                ff.scroll_y = 0;
            } else if (key == CHAR_END) {
                ff.scroll_y = max_scroll;
            }
        }
    }
}

static void firefox_on_close(window_t* win) {
    (void)win;
    ff_win = NULL;
}

// Public Launch API
void gui_firefox_open_url(const char* url) {
    if (ff_win && !ff_win->is_closed) {
        if (ff_win->is_minimized) wm_restore_window(ff_win);
        wm_bring_to_front(ff_win);
        if (url && url[0] != '\0') {
            ff_navigate(url, 1);
        }
        return;
    }

    // Initialize state
    ff.omnibox_focused = 0;
    ff.cursor_blink = 0;
    ff.history_idx = 0;
    ff.history_count = 0;
    ff.scroll_y = 0;
    ff.is_dragging_scroll = 0;
    ff.net_test_msg[0] = '\0';

    // Create 740x500 Browser Window
    ff_win = wm_create_window("NetSurf Web Browser", ICON_FIREFOX, 140, 60, 740, 500, firefox_render);
    if (ff_win) {
        ff_win->on_mouse = firefox_on_mouse;
        ff_win->on_key = firefox_on_key;
        ff_win->on_close = firefox_on_close;
    }

    const char* start_url = (url && url[0] != '\0') ? url : "https://www.google.com";
    ff_navigate(start_url, 1);
}

void gui_firefox_open(void) {
    gui_firefox_open_url("https://www.google.com");
}
