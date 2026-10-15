#include "netsurf.h"
#include "graphics.h"
#include "font.h"
#include "string.h"
#include "serial.h"
#include <stdint.h>
#include <stddef.h>

// Static allocations for embedded baremetal efficiency
static ns_node_t dom_nodes[NS_MAX_NODES];
static int num_nodes = 0;
static ns_node_t* dom_root = NULL;

static ns_box_t layout_boxes[NS_MAX_BOXES];
static int num_boxes = 0;
static int total_page_height = 0;

static char page_title[64];

// String helpers
static int ns_strlen(const char* s) {
    int len = 0;
    while (s && s[len]) len++;
    return len;
}

static void ns_strcpy(char* d, const char* s, int max) {
    int i = 0;
    while (s && s[i] && i < max - 1) { d[i] = s[i]; i++; }
    d[i] = '\0';
}

static int ns_strcmp(const char* a, const char* b) {
    while (*a && (*a == *b)) { a++; b++; }
    return *(const unsigned char*)a - *(const unsigned char*)b;
}

static int ns_strcasecmp(const char* a, const char* b) {
    while (*a && *b) {
        char ca = (*a >= 'A' && *a <= 'Z') ? (*a + 32) : *a;
        char cb = (*b >= 'A' && *b <= 'Z') ? (*b + 32) : *b;
        if (ca != cb) return (unsigned char)ca - (unsigned char)cb;
        a++; b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

static const char* ns_strstr(const char* haystack, const char* needle) {
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

static uint32_t ns_parse_hex_color(const char* hex) {
    if (!hex) return 0xFFCDD6F4;
    while (*hex == ' ' || *hex == '#') hex++;
    uint32_t val = 0;
    int count = 0;
    while (*hex && count < 6) {
        val <<= 4;
        if (*hex >= '0' && *hex <= '9') val |= (*hex - '0');
        else if (*hex >= 'a' && *hex <= 'f') val |= (*hex - 'a' + 10);
        else if (*hex >= 'A' && *hex <= 'F') val |= (*hex - 'A' + 10);
        hex++;
        count++;
    }
    return 0xFF000000 | val;
}

static int ns_parse_int(const char* s) {
    while (*s && (*s < '0' || *s > '9')) s++;
    int val = 0;
    while (*s >= '0' && *s <= '9') {
        val = val * 10 + (*s - '0');
        s++;
    }
    return val;
}

// Tag identification & default styles (NetSurf User-Agent CSS)
static ns_tag_t ns_identify_tag(const char* name) {
    if (ns_strcasecmp(name, "html") == 0) return NS_TAG_HTML;
    if (ns_strcasecmp(name, "head") == 0) return NS_TAG_HEAD;
    if (ns_strcasecmp(name, "title") == 0) return NS_TAG_TITLE;
    if (ns_strcasecmp(name, "style") == 0) return NS_TAG_STYLE;
    if (ns_strcasecmp(name, "body") == 0) return NS_TAG_BODY;
    if (ns_strcasecmp(name, "div") == 0) return NS_TAG_DIV;
    if (ns_strcasecmp(name, "section") == 0) return NS_TAG_SECTION;
    if (ns_strcasecmp(name, "header") == 0) return NS_TAG_HEADER;
    if (ns_strcasecmp(name, "footer") == 0) return NS_TAG_FOOTER;
    if (ns_strcasecmp(name, "p") == 0) return NS_TAG_P;
    if (ns_strcasecmp(name, "h1") == 0) return NS_TAG_H1;
    if (ns_strcasecmp(name, "h2") == 0) return NS_TAG_H2;
    if (ns_strcasecmp(name, "h3") == 0) return NS_TAG_H3;
    if (ns_strcasecmp(name, "h4") == 0) return NS_TAG_H4;
    if (ns_strcasecmp(name, "a") == 0) return NS_TAG_A;
    if (ns_strcasecmp(name, "span") == 0) return NS_TAG_SPAN;
    if (ns_strcasecmp(name, "b") == 0 || ns_strcasecmp(name, "strong") == 0) return NS_TAG_B;
    if (ns_strcasecmp(name, "i") == 0 || ns_strcasecmp(name, "em") == 0) return NS_TAG_I;
    if (ns_strcasecmp(name, "u") == 0 || ns_strcasecmp(name, "small") == 0) return NS_TAG_SPAN;
    if (ns_strcasecmp(name, "pre") == 0) return NS_TAG_PRE;
    if (ns_strcasecmp(name, "code") == 0) return NS_TAG_CODE;
    if (ns_strcasecmp(name, "ul") == 0) return NS_TAG_UL;
    if (ns_strcasecmp(name, "ol") == 0) return NS_TAG_OL;
    if (ns_strcasecmp(name, "li") == 0) return NS_TAG_LI;
    if (ns_strcasecmp(name, "table") == 0) return NS_TAG_TABLE;
    if (ns_strcasecmp(name, "tr") == 0) return NS_TAG_TR;
    if (ns_strcasecmp(name, "th") == 0) return NS_TAG_TH;
    if (ns_strcasecmp(name, "td") == 0) return NS_TAG_TD;
    if (ns_strcasecmp(name, "button") == 0) return NS_TAG_BUTTON;
    if (ns_strcasecmp(name, "input") == 0) return NS_TAG_INPUT;
    if (ns_strcasecmp(name, "hr") == 0) return NS_TAG_HR;
    if (ns_strcasecmp(name, "br") == 0) return NS_TAG_BR;
    if (ns_strcasecmp(name, "img") == 0) return NS_TAG_IMG;
    return NS_TAG_UNKNOWN;
}

static void ns_apply_default_style(ns_node_t* node) {
    ns_style_t* s = &node->style;
    s->color = COLOR_TEXT_PRIMARY;
    s->bg_color = 0x00000000; // transparent
    s->border_color = 0x00000000;
    s->border_width = 0;
    s->border_radius = 0;
    s->font_size = 14;
    s->font_bold = 0;
    s->text_align = 0; // left
    s->display = 1;    // block
    s->margin_top = s->margin_bottom = s->margin_left = s->margin_right = 0;
    s->padding_top = s->padding_bottom = s->padding_left = s->padding_right = 0;
    s->width = s->height = -1;

    switch (node->tag) {
        case NS_TAG_BODY:
            s->bg_color = 0xFF181825;
            s->padding_top = s->padding_bottom = s->padding_left = s->padding_right = 16;
            break;
        case NS_TAG_H1:
            s->color = COLOR_LEMON_GOLD;
            s->font_size = 20;
            s->font_bold = 1;
            s->margin_top = 14; s->margin_bottom = 10;
            break;
        case NS_TAG_H2:
            s->color = 0xFFFAB387; // Warm orange
            s->font_size = 18;
            s->font_bold = 1;
            s->margin_top = 12; s->margin_bottom = 8;
            break;
        case NS_TAG_H3:
            s->color = COLOR_BLUE_ACCENT;
            s->font_size = 16;
            s->font_bold = 1;
            s->margin_top = 10; s->margin_bottom = 6;
            break;
        case NS_TAG_P:
            s->margin_top = 6; s->margin_bottom = 6;
            break;
        case NS_TAG_A:
            s->color = COLOR_BLUE_ACCENT;
            s->display = 0; // inline
            break;
        case NS_TAG_SPAN:
        case NS_TAG_B:
        case NS_TAG_STRONG:
        case NS_TAG_EM:
        case NS_TAG_I:
            s->display = 0; // inline
            if (node->tag == NS_TAG_B || node->tag == NS_TAG_STRONG) s->font_bold = 1;
            break;
        case NS_TAG_PRE:
        case NS_TAG_CODE:
            s->bg_color = 0xFF11111B;
            s->border_color = 0xFF313244;
            s->border_width = 1;
            s->border_radius = 6;
            s->color = COLOR_GREEN_ACCENT;
            s->padding_top = s->padding_bottom = s->padding_left = s->padding_right = 8;
            s->margin_top = 8; s->margin_bottom = 8;
            break;
        case NS_TAG_TABLE:
            s->display = 3; // table
            s->bg_color = 0xFF1E1E2E;
            s->border_color = 0xFF45475A;
            s->border_width = 1;
            s->border_radius = 6;
            s->margin_top = 8; s->margin_bottom = 8;
            break;
        case NS_TAG_TR:
            s->display = 4; // table-row
            break;
        case NS_TAG_TH:
            s->display = 5; // table-cell
            s->color = COLOR_LEMON_GOLD;
            s->font_bold = 1;
            s->bg_color = 0xFF24273A;
            s->padding_top = s->padding_bottom = 6;
            s->padding_left = s->padding_right = 10;
            s->border_color = 0xFF45475A;
            s->border_width = 1;
            break;
        case NS_TAG_TD:
            s->display = 5; // table-cell
            s->padding_top = s->padding_bottom = 6;
            s->padding_left = s->padding_right = 10;
            s->border_color = 0xFF313244;
            s->border_width = 1;
            break;
        case NS_TAG_HR:
            s->border_color = 0xFF45475A;
            s->border_width = 1;
            s->margin_top = 10; s->margin_bottom = 10;
            s->height = 1;
            break;
        case NS_TAG_BUTTON:
            s->display = 2; // inline-block
            s->bg_color = 0xFF24273A;
            s->border_color = COLOR_LEMON_GOLD;
            s->border_width = 1;
            s->border_radius = 6;
            s->color = COLOR_LEMON_GOLD;
            s->padding_top = s->padding_bottom = 6;
            s->padding_left = s->padding_right = 0;
            s->margin_top = s->margin_bottom = 4;
            s->font_bold = 1;
            break;
        case NS_TAG_LI:
            s->margin_top = 3; s->margin_bottom = 3;
            s->padding_left = 16; // bullet indent
            break;
        case NS_TAG_HEAD:
        case NS_TAG_TITLE:
        case NS_TAG_STYLE:
            s->display = 6; // none
            break;
        default:
            break;
    }

    // Check class names (e.g. class="card", class="status", etc.)
    if (ns_strcmp(node->class_name, "card") == 0) {
        s->bg_color = 0xFF1E1E2E;
        s->border_color = 0xFF45475A;
        s->border_width = 1;
        s->border_radius = 8;
        s->padding_top = s->padding_bottom = 6;
        s->padding_left = s->padding_right = 10;
        s->margin_top = 4; s->margin_bottom = 4;
    } else if (ns_strcmp(node->class_name, "hero") == 0) {
        s->bg_color = 0xFF1E1E2E;
        s->border_color = 0xFF89B4FA;
        s->border_width = 1;
        s->border_radius = 8;
        s->padding_top = s->padding_bottom = s->padding_left = s->padding_right = 12;
        s->margin_top = 6; s->margin_bottom = 6;
    } else if (ns_strcmp(node->class_name, "status") == 0) {
        s->bg_color = 0xFF11111B;
        s->border_color = 0xFF313244;
        s->border_width = 1;
        s->border_radius = 4;
        s->color = COLOR_GREEN_ACCENT;
        s->padding_top = s->padding_bottom = 4;
        s->padding_left = s->padding_right = 8;
        s->margin_top = 4; s->margin_bottom = 4;
    } else if (ns_strcmp(node->class_name, "box") == 0) {
        s->bg_color = 0xFF1E1E2E;
        s->border_color = 0xFF45475A;
        s->border_width = 1;
        s->border_radius = 8;
        s->padding_top = s->padding_bottom = s->padding_left = s->padding_right = 12;
        s->margin_top = 8; s->margin_bottom = 8;
    } else if (ns_strcmp(node->class_name, "file") == 0) {
        s->bg_color = 0xFF181825;
        s->border_color = 0xFF45475A;
        s->border_width = 1;
        s->border_radius = 6;
        s->padding_top = s->padding_bottom = 4;
        s->padding_left = s->padding_right = 8;
        s->margin_top = 3; s->margin_bottom = 3;
    } else if (ns_strcmp(node->class_name, "searchbar") == 0 || ns_strcmp(node->class_name, "search-pill") == 0) {
        s->bg_color = 0xFF202124;
        s->border_color = 0xFF5F6368;
        s->border_width = 1;
        s->border_radius = 20;
        s->padding_top = s->padding_bottom = 8;
        s->padding_left = s->padding_right = 16;
        s->margin_top = 6; s->margin_bottom = 6;
    } else if (ns_strcmp(node->class_name, "search-pill-sm") == 0) {
        s->bg_color = 0xFF202124;
        s->border_color = 0xFF5F6368;
        s->border_width = 1;
        s->border_radius = 16;
        s->padding_top = s->padding_bottom = 4;
        s->padding_left = s->padding_right = 12;
    } else if (ns_strcmp(node->class_name, "g-btn") == 0) {
        s->bg_color = 0xFF303134;
        s->border_color = 0xFF5F6368;
        s->border_width = 1;
        s->border_radius = 4;
        s->color = 0xFFE8EAED;
        s->padding_top = s->padding_bottom = 6;
        s->padding_left = s->padding_right = 14;
        s->margin_left = s->margin_right = 4;
    } else if (ns_strcmp(node->class_name, "btn-signin") == 0) {
        s->bg_color = 0xFF1A73E8;
        s->border_color = 0xFF1A73E8;
        s->border_width = 1;
        s->border_radius = 4;
        s->color = 0xFFFFFFFF;
        s->padding_top = s->padding_bottom = 6;
        s->padding_left = s->padding_right = 16;
        s->font_bold = 1;
    } else if (ns_strcmp(node->class_name, "result") == 0) {
        s->bg_color = 0xFF181825;
        s->border_color = 0xFF313244;
        s->border_width = 1;
        s->border_radius = 6;
        s->padding_top = s->padding_bottom = 6;
        s->padding_left = s->padding_right = 12;
        s->margin_top = 6; s->margin_bottom = 6;
    } else if (ns_strcmp(node->class_name, "kpanel") == 0) {
        s->bg_color = 0xFF1E1E2E;
        s->border_color = 0xFF89B4FA;
        s->border_width = 1;
        s->border_radius = 8;
        s->padding_top = s->padding_bottom = 10;
        s->padding_left = s->padding_right = 14;
        s->margin_top = 6; s->margin_bottom = 8;
    } else if (ns_strcmp(node->class_name, "header-bar") == 0) {
        s->bg_color = 0xFF1E1E2E;
        s->border_color = 0xFF313244;
        s->border_width = 1;
        s->border_radius = 6;
        s->padding_top = s->padding_bottom = 6;
        s->padding_left = s->padding_right = 12;
        s->margin_top = 2; s->margin_bottom = 8;
    } else if (ns_strcmp(node->class_name, "center-box") == 0) {
        s->bg_color = 0xFF181825;
        s->border_color = 0xFF45475A;
        s->border_width = 1;
        s->border_radius = 8;
        s->padding_top = s->padding_bottom = 14;
        s->padding_left = s->padding_right = 16;
        s->margin_top = 6; s->margin_bottom = 8;
    } else if (ns_strcmp(node->class_name, "infobox") == 0) {
        s->bg_color = 0xFF1E1E2E;
        s->border_color = 0xFF45475A;
        s->border_width = 1;
        s->border_radius = 6;
        s->margin_top = 8; s->margin_bottom = 8;
    } else if (ns_strcmp(node->class_name, "toc") == 0) {
        s->bg_color = 0xFF181825;
        s->border_color = 0xFF313244;
        s->border_width = 1;
        s->border_radius = 6;
        s->padding_top = s->padding_bottom = 8;
        s->padding_left = s->padding_right = 12;
        s->margin_top = 6; s->margin_bottom = 8;
    }
}

static void ns_parse_inline_style(ns_node_t* node, const char* style_str) {
    if (!style_str) return;
    const char* p = style_str;
    while (*p) {
        while (*p == ' ' || *p == ';') p++;
        if (!*p) break;

        char key[32], val[64];
        int kp = 0, vp = 0;
        while (*p && *p != ':' && *p != ';' && kp < 31) key[kp++] = *p++;
        key[kp] = '\0';
        if (*p == ':') p++;
        while (*p == ' ') p++;
        while (*p && *p != ';' && vp < 63) val[vp++] = *p++;
        val[vp] = '\0';

        if (ns_strcasecmp(key, "color") == 0) {
            node->style.color = ns_parse_hex_color(val);
        } else if (ns_strcasecmp(key, "background") == 0 || ns_strcasecmp(key, "background-color") == 0) {
            node->style.bg_color = ns_parse_hex_color(val);
        } else if (ns_strcasecmp(key, "border-color") == 0) {
            node->style.border_color = ns_parse_hex_color(val);
        } else if (ns_strcasecmp(key, "border") == 0 || ns_strcasecmp(key, "border-bottom") == 0 || ns_strcasecmp(key, "border-top") == 0) {
            if (ns_strstr(val, "#")) {
                node->style.border_color = ns_parse_hex_color(ns_strstr(val, "#"));
                node->style.border_width = 1;
            }
        } else if (ns_strcasecmp(key, "border-radius") == 0) {
            node->style.border_radius = ns_parse_int(val);
        } else if (ns_strcasecmp(key, "padding") == 0) {
            int pad = ns_parse_int(val);
            node->style.padding_top = node->style.padding_bottom = node->style.padding_left = node->style.padding_right = pad;
        } else if (ns_strcasecmp(key, "padding-left") == 0) {
            node->style.padding_left = ns_parse_int(val);
        } else if (ns_strcasecmp(key, "padding-right") == 0) {
            node->style.padding_right = ns_parse_int(val);
        } else if (ns_strcasecmp(key, "padding-top") == 0) {
            node->style.padding_top = ns_parse_int(val);
        } else if (ns_strcasecmp(key, "padding-bottom") == 0) {
            node->style.padding_bottom = ns_parse_int(val);
        } else if (ns_strcasecmp(key, "margin") == 0) {
            int mar = ns_parse_int(val);
            node->style.margin_top = node->style.margin_bottom = node->style.margin_left = node->style.margin_right = mar;
        } else if (ns_strcasecmp(key, "margin-left") == 0) {
            node->style.margin_left = ns_parse_int(val);
        } else if (ns_strcasecmp(key, "margin-right") == 0) {
            node->style.margin_right = ns_parse_int(val);
        } else if (ns_strcasecmp(key, "margin-top") == 0) {
            node->style.margin_top = ns_parse_int(val);
        } else if (ns_strcasecmp(key, "margin-bottom") == 0) {
            node->style.margin_bottom = ns_parse_int(val);
        } else if (ns_strcasecmp(key, "display") == 0) {
            if (ns_strcasecmp(val, "block") == 0) node->style.display = 1;
            else if (ns_strcasecmp(val, "inline") == 0) node->style.display = 0;
            else if (ns_strcasecmp(val, "inline-block") == 0) node->style.display = 2;
            else if (ns_strcasecmp(val, "none") == 0) node->style.display = 6;
        } else if (ns_strcasecmp(key, "width") == 0) {
            node->style.width = ns_parse_int(val);
        } else if (ns_strcasecmp(key, "height") == 0) {
            node->style.height = ns_parse_int(val);
        } else if (ns_strcasecmp(key, "font-size") == 0) {
            node->style.font_size = ns_parse_int(val);
        } else if (ns_strcasecmp(key, "text-align") == 0) {
            if (ns_strcasecmp(val, "center") == 0) node->style.text_align = 1;
            else if (ns_strcasecmp(val, "right") == 0) node->style.text_align = 2;
            else node->style.text_align = 0;
        } else if (ns_strcasecmp(key, "font-weight") == 0) {
            if (ns_strcasecmp(val, "bold") == 0) node->style.font_bold = 1;
        }
    }
}

// DOM Node Management
static ns_node_t* ns_alloc_node(ns_node_type_t type, ns_tag_t tag) {
    if (num_nodes >= NS_MAX_NODES) return NULL;
    ns_node_t* n = &dom_nodes[num_nodes];
    n->id = num_nodes++;
    n->type = type;
    n->tag = tag;
    n->tag_name[0] = '\0';
    n->elem_id[0] = '\0';
    n->class_name[0] = '\0';
    n->href[0] = '\0';
    n->src[0] = '\0';
    n->text[0] = '\0';
    n->action_id = 0;
    n->parent = NULL;
    n->first_child = NULL;
    n->last_child = NULL;
    n->next_sibling = NULL;
    n->prev_sibling = NULL;

    ns_apply_default_style(n);
    return n;
}

static void ns_append_child(ns_node_t* parent, ns_node_t* child) {
    if (!parent || !child) return;
    child->parent = parent;
    child->next_sibling = NULL;
    child->prev_sibling = parent->last_child;
    if (parent->last_child) {
        parent->last_child->next_sibling = child;
    } else {
        parent->first_child = child;
    }
    parent->last_child = child;
}

// HTML5 Tokenizer & DOM Parser (NetSurf Hubbub Engine)
void netsurf_init(void) {
    num_nodes = 0;
    num_boxes = 0;
    dom_root = NULL;
    total_page_height = 0;
    ns_strcpy(page_title, "NetSurf Browser", sizeof(page_title));
}

void netsurf_parse_html(const char* html, int len) {
    netsurf_init();
    if (!html || len <= 0) return;

    dom_root = ns_alloc_node(NS_NODE_ELEMENT, NS_TAG_HTML);
    ns_strcpy(dom_root->tag_name, "html", 16);

    ns_node_t* current = dom_root;
    int i = 0;

    while (i < len) {
        if (html[i] == '<') {
            i++;
            if (i >= len) break;

            // Comment <!-- ... -->
            if (i + 2 < len && html[i] == '!' && html[i+1] == '-' && html[i+2] == '-') {
                i += 3;
                while (i + 2 < len && !(html[i] == '-' && html[i+1] == '-' && html[i+2] == '>')) i++;
                i += 3;
                continue;
            }

            // Closing tag </tag>
            if (html[i] == '/') {
                i++;
                while (i < len && html[i] != '>') i++;
                if (i < len && html[i] == '>') i++;

                if (current && current->parent) {
                    current = current->parent;
                }
                continue;
            }

            // Opening tag <tag ...>
            char otag[24];
            int op = 0;
            while (i < len && html[i] != '>' && html[i] != ' ' && html[i] != '/' && op < 23) {
                otag[op++] = html[i++];
            }
            otag[op] = '\0';

            ns_tag_t tag_id = ns_identify_tag(otag);
            ns_node_t* new_node = ns_alloc_node(NS_NODE_ELEMENT, tag_id);
            if (!new_node) break;
            ns_strcpy(new_node->tag_name, otag, 16);

            // Parse attributes
            int is_self_closing = (tag_id == NS_TAG_HR || tag_id == NS_TAG_BR || tag_id == NS_TAG_IMG || tag_id == NS_TAG_INPUT);

            while (i < len && html[i] != '>') {
                while (i < len && (html[i] == ' ' || html[i] == '\t' || html[i] == '\r' || html[i] == '\n')) i++;
                if (i >= len || html[i] == '>') break;
                if (html[i] == '/') { is_self_closing = 1; i++; continue; }

                char aname[32], aval[128];
                int anp = 0, avp = 0;
                while (i < len && html[i] != '=' && html[i] != ' ' && html[i] != '>' && anp < 31) aname[anp++] = html[i++];
                aname[anp] = '\0';
                if (i < len && html[i] == '=') {
                    i++;
                    char quote = (i < len && (html[i] == '\"' || html[i] == '\'')) ? html[i++] : 0;
                    while (i < len && (quote ? (html[i] != quote) : (html[i] != ' ' && html[i] != '>')) && avp < 127) {
                        aval[avp++] = html[i++];
                    }
                    if (quote && i < len && html[i] == quote) i++;
                }
                aval[avp] = '\0';

                if (ns_strcasecmp(aname, "href") == 0) {
                    ns_strcpy(new_node->href, aval, NS_MAX_URL_LEN);
                } else if (ns_strcasecmp(aname, "src") == 0) {
                    ns_strcpy(new_node->src, aval, NS_MAX_URL_LEN);
                } else if (ns_strcasecmp(aname, "class") == 0) {
                    ns_strcpy(new_node->class_name, aval, 32);
                    ns_apply_default_style(new_node);
                } else if (ns_strcasecmp(aname, "id") == 0) {
                    ns_strcpy(new_node->elem_id, aval, 24);
                } else if (ns_strcasecmp(aname, "action") == 0 || ns_strcasecmp(aname, "data-action") == 0) {
                    new_node->action_id = ns_parse_int(aval);
                } else if (ns_strcasecmp(aname, "style") == 0) {
                    ns_parse_inline_style(new_node, aval);
                }
            }
            if (i < len && html[i] == '>') i++;

            ns_append_child(current, new_node);
            if (!is_self_closing) {
                current = new_node;
            }
        } else {
            // Text Node content
            char text_buf[NS_MAX_TEXT_LEN];
            int tp = 0;
            while (i < len && html[i] != '<' && tp < NS_MAX_TEXT_LEN - 1) {
                char ch = html[i++];
                if (ch == '&') {
                    if (i + 2 < len && html[i] == 'l' && html[i+1] == 't' && html[i+2] == ';') {
                        ch = '<'; i += 3;
                    } else if (i + 2 < len && html[i] == 'g' && html[i+1] == 't' && html[i+2] == ';') {
                        ch = '>'; i += 3;
                    } else if (i + 3 < len && html[i] == 'a' && html[i+1] == 'm' && html[i+2] == 'p' && html[i+3] == ';') {
                        ch = '&'; i += 4;
                    } else if (i + 4 < len && html[i] == 'n' && html[i+1] == 'b' && html[i+2] == 's' && html[i+3] == 'p' && html[i+4] == ';') {
                        ch = ' '; i += 5;
                    }
                }
                if (ch == '\r' || ch == '\n' || ch == '\t') ch = ' ';
                // Collapse consecutive spaces
                if (ch == ' ' && tp > 0 && text_buf[tp - 1] == ' ') continue;
                text_buf[tp++] = ch;
            }
            text_buf[tp] = '\0';

            // Check if inside <title>
            if (current && current->tag == NS_TAG_TITLE) {
                ns_strcpy(page_title, text_buf, sizeof(page_title));
            } else if (tp > 0) {
                if (tp == 1 && text_buf[0] == ' ') {
                    if (!current || current->tag == NS_TAG_HTML || current->tag == NS_TAG_BODY || current->tag == NS_TAG_TABLE || current->tag == NS_TAG_TR || current->tag == NS_TAG_HEAD) {
                        continue;
                    }
                }
                ns_node_t* tnode = ns_alloc_node(NS_NODE_TEXT, NS_TAG_UNKNOWN);
                if (tnode) {
                    ns_strcpy(tnode->text, text_buf, NS_MAX_TEXT_LEN);
                    tnode->style = current->style;
                    tnode->style.display = 0; // inline
                    // Text nodes do not render container backgrounds, borders, or margins
                    tnode->style.bg_color = 0;
                    tnode->style.border_width = 0;
                    tnode->style.border_color = 0;
                    tnode->style.border_radius = 0;
                    tnode->style.margin_top = tnode->style.margin_bottom = 0;
                    tnode->style.margin_left = tnode->style.margin_right = 0;
                    tnode->style.padding_top = tnode->style.padding_bottom = 0;
                    tnode->style.padding_left = tnode->style.padding_right = 0;
                    ns_append_child(current, tnode);
                }
            }
        }
    }
}

// NetSurf Layout Engine (Box Tree & Formatting Context)
static void ns_wrap_text(const char* text, int max_w, char lines[32][128], int* line_count) {
    *line_count = 0;
    if (!text || text[0] == '\0') return;

    int max_chars_per_line = max_w / 8;
    if (max_chars_per_line > 120) max_chars_per_line = 120;
    if (max_chars_per_line < 5) max_chars_per_line = 5;

    int p = 0;
    int len = ns_strlen(text);

    while (p < len && *line_count < 32) {
        while (p < len && text[p] == ' ') p++;
        if (p >= len) break;

        // Check if there is an explicit newline
        int newline_idx = -1;
        for (int k = 0; k < max_chars_per_line && (p + k) < len; k++) {
            if (text[p + k] == '\n') {
                newline_idx = k;
                break;
            }
        }

        int take;
        if (newline_idx != -1) {
            take = newline_idx;
        } else {
            take = len - p;
            if (take > max_chars_per_line) {
                take = max_chars_per_line;
                // Break at word boundary
                int last_space = take;
                while (last_space > 0 && text[p + last_space] != ' ') last_space--;
                if (last_space > 0) take = last_space;
            }
        }

        int lp = 0;
        for (int k = 0; k < take && lp < 127; k++) {
            lines[*line_count][lp++] = text[p++];
        }
        lines[*line_count][lp] = '\0';
        (*line_count)++;

        if (newline_idx != -1 && p < len && text[p] == '\n') {
            p++;
        }
    }
}

static void ns_build_boxes(ns_node_t* node, int container_x, int* cur_y, int container_w) {
    if (!node) return;
    if (node->style.display == 6) return; // display: none

    if (num_boxes >= NS_MAX_BOXES) return;

    ns_box_t* box = NULL;

    if (node->type == NS_NODE_TEXT) {
        if (node->text[0] != '\0') {
            box = &layout_boxes[num_boxes++];
            box->node = node;
            box->style = node->style;
            box->x = container_x;
            box->y = *cur_y;
            box->width = container_w;
            box->is_link = (node->parent && node->parent->tag == NS_TAG_A);
            box->is_button = (node->parent && node->parent->tag == NS_TAG_BUTTON);
            box->action_id = (node->parent) ? node->parent->action_id : 0;

            ns_wrap_text(node->text, container_w, box->lines, &box->line_count);
            box->height = (box->line_count > 0) ? (box->line_count * (box->style.font_size + 2)) : 0;
            *cur_y += box->height;
        }
    } else {
        // Element Block
        int box_idx = num_boxes++;
        box = &layout_boxes[box_idx];
        box->node = node;
        box->style = node->style;
        box->x = container_x + node->style.margin_left;
        box->y = *cur_y + node->style.margin_top;
        box->width = (node->style.width > 0) ? node->style.width : (container_w - node->style.margin_left - node->style.margin_right);
        box->is_link = (node->tag == NS_TAG_A);
        box->is_button = (node->tag == NS_TAG_BUTTON);
        box->is_input = (node->tag == NS_TAG_INPUT);
        box->action_id = node->action_id;
        box->line_count = 0;

        int inner_x = box->x + node->style.padding_left;
        int inner_y = box->y + node->style.padding_top;
        int inner_w = box->width - node->style.padding_left - node->style.padding_right;

        int start_inner_y = inner_y;

        // Traverse children
        if (node->tag == NS_TAG_TR) {
            int cell_count = 0;
            ns_node_t* c = node->first_child;
            while (c) {
                if (c->tag == NS_TAG_TD || c->tag == NS_TAG_TH) cell_count++;
                c = c->next_sibling;
            }
            if (cell_count > 0) {
                int row_max_h = 22;
                int cx = inner_x;
                c = node->first_child;
                int idx = 0;
                while (c) {
                    if (c->tag == NS_TAG_TD || c->tag == NS_TAG_TH) {
                        int cell_w;
                        if (cell_count == 1) {
                            cell_w = inner_w;
                        } else if (cell_count == 2) {
                            cell_w = (idx == 0) ? ((inner_w * 36) / 100) : (inner_w - ((inner_w * 36) / 100));
                        } else {
                            cell_w = inner_w / cell_count;
                        }
                        int cell_y = inner_y;
                        c->style.width = cell_w;
                        ns_build_boxes(c, cx, &cell_y, cell_w);
                        int ch = cell_y - inner_y;
                        if (ch > row_max_h) row_max_h = ch;
                        cx += cell_w;
                        idx++;
                    } else {
                        ns_build_boxes(c, inner_x, &inner_y, inner_w);
                    }
                    c = c->next_sibling;
                }
                inner_y += row_max_h;
            }
        } else {
            // Check if all children are inline or buttons
            int all_inline = 1;
            ns_node_t* chk = node->first_child;
            while (chk) {
                if (chk->type == NS_NODE_ELEMENT && chk->style.display == 1 && chk->tag != NS_TAG_BR && chk->tag != NS_TAG_HR) {
                    all_inline = 0;
                    break;
                }
                chk = chk->next_sibling;
            }

            if (all_inline && node->first_child) {
                // Inline flow for text, links, spans, buttons with text-align support
                ns_node_t* line_start = node->first_child;
                int cur_line_y = inner_y;

                while (line_start) {
                    if (line_start->tag == NS_TAG_BR) {
                        cur_line_y += (node->style.font_size > 0 ? (node->style.font_size + 4) : 18);
                        line_start = line_start->next_sibling;
                        continue;
                    }

                    // Pass 1: find how many children fit on this line and calculate row width
                    ns_node_t* c = line_start;
                    int row_w = 0;
                    int line_h = (node->style.font_size > 0 ? (node->style.font_size + 4) : 18);
                    ns_node_t* line_end = NULL;

                    while (c) {
                        if (c->tag == NS_TAG_BR) {
                            line_end = c;
                            break;
                        }

                        int item_w = 0;
                        int item_h = line_h;

                        if (c->type == NS_NODE_TEXT) {
                            int tlen = ns_strlen(c->text);
                            item_w = tlen * 8;
                            if (row_w + item_w > inner_w && row_w > 0) {
                                line_end = c;
                                break;
                            }
                            if (row_w == 0 && item_w > inner_w) {
                                line_end = c->next_sibling;
                                row_w = inner_w;
                                break;
                            }
                        } else if (c->tag == NS_TAG_BUTTON) {
                            int txt_len = 0;
                            if (c->first_child && c->first_child->type == NS_NODE_TEXT) {
                                txt_len = ns_strlen(c->first_child->text);
                            }
                            int pad_w = c->style.padding_left + c->style.padding_right;
                            if (pad_w < 16) pad_w = 16;
                            item_w = txt_len * 8 + pad_w + 12;
                            if (c->style.width > 0) item_w = c->style.width;
                            item_w += 8;
                            item_h = 28;
                            if (row_w + item_w > inner_w && row_w > 0) {
                                line_end = c;
                                break;
                            }
                        } else {
                            // Inline elements: a, span, b, strong, em, i
                            int txt_len = 0;
                            if (c->first_child && c->first_child->type == NS_NODE_TEXT) {
                                txt_len = ns_strlen(c->first_child->text);
                            }
                            int char_w = (c->style.font_size >= 24) ? 16 : 8;
                            item_w = txt_len * char_w;
                            if (c->style.font_size >= 24) item_h = 36;
                            int mr = c->style.margin_right > 0 ? c->style.margin_right : (c->next_sibling ? 6 : 0);
                            item_w += mr;
                            if (row_w + item_w > inner_w && row_w > 0) {
                                line_end = c;
                                break;
                            }
                        }

                        if (item_h > line_h) line_h = item_h;
                        row_w += item_w;
                        c = c->next_sibling;
                    }

                    // Pass 2: calculate start_x according to text_align
                    int start_x = inner_x;
                    if (node->style.text_align == 1) { // Center
                        start_x = inner_x + ((inner_w > row_w) ? (inner_w - row_w) / 2 : 0);
                    } else if (node->style.text_align == 2) { // Right
                        start_x = inner_x + ((inner_w > row_w) ? (inner_w - row_w) : 0);
                    }

                    // Pass 3: emit boxes for children on this line
                    int curr_x = start_x;
                    c = line_start;
                    while (c != line_end && c) {
                        if (c->tag == NS_TAG_BR) break;

                        if (c->type == NS_NODE_TEXT) {
                            if (c->text[0] != '\0') {
                                int tlen = ns_strlen(c->text);
                                int text_w = tlen * 8;
                                if (text_w <= inner_w) {
                                    if (num_boxes < NS_MAX_BOXES) {
                                        ns_box_t* b = &layout_boxes[num_boxes++];
                                        b->node = c;
                                        b->style = c->style;
                                        b->x = curr_x;
                                        b->y = cur_line_y;
                                        b->width = text_w;
                                        b->height = line_h;
                                        b->is_link = (c->parent && c->parent->tag == NS_TAG_A);
                                        b->is_button = (c->parent && c->parent->tag == NS_TAG_BUTTON);
                                        b->action_id = (c->parent) ? c->parent->action_id : 0;
                                        ns_strcpy(b->lines[0], c->text, 128);
                                        b->line_count = 1;
                                    }
                                    curr_x += text_w;
                                } else {
                                    // Wrap long multiline text
                                    if (num_boxes < NS_MAX_BOXES) {
                                        ns_box_t* b = &layout_boxes[num_boxes++];
                                        b->node = c;
                                        b->style = c->style;
                                        b->x = inner_x;
                                        b->y = cur_line_y;
                                        b->width = inner_w;
                                        b->is_link = (c->parent && c->parent->tag == NS_TAG_A);
                                        b->is_button = (c->parent && c->parent->tag == NS_TAG_BUTTON);
                                        b->action_id = (c->parent) ? c->parent->action_id : 0;
                                        ns_wrap_text(c->text, inner_w, b->lines, &b->line_count);
                                        b->height = (b->line_count > 0) ? (b->line_count * (b->style.font_size + 2)) : line_h;
                                        line_h = b->height;
                                    }
                                    curr_x += inner_w;
                                }
                            }
                        } else if (c->tag == NS_TAG_BUTTON) {
                            int txt_len = 0;
                            if (c->first_child && c->first_child->type == NS_NODE_TEXT) {
                                txt_len = ns_strlen(c->first_child->text);
                            }
                            int pad_w = c->style.padding_left + c->style.padding_right;
                            if (pad_w < 16) pad_w = 16;
                            int bw = txt_len * 8 + pad_w + 12;
                            if (c->style.width > 0) bw = c->style.width;
                            int by = cur_line_y;
                            c->style.width = bw;
                            ns_build_boxes(c, curr_x, &by, bw);
                            curr_x += bw + 8;
                            if (by - cur_line_y > line_h) line_h = by - cur_line_y;
                        } else {
                            // Inline element (a, span, b, strong, em, i)
                            if (c->first_child && c->first_child->type == NS_NODE_TEXT && !c->first_child->next_sibling) {
                                int tlen = ns_strlen(c->first_child->text);
                                int char_w = (c->style.font_size >= 24) ? 16 : 8;
                                int text_w = tlen * char_w;
                                if (num_boxes < NS_MAX_BOXES) {
                                    ns_box_t* b = &layout_boxes[num_boxes++];
                                    b->node = c->first_child;
                                    b->style = c->style;
                                    b->x = curr_x;
                                    b->y = cur_line_y;
                                    b->width = text_w;
                                    b->height = line_h;
                                    b->is_link = (c->tag == NS_TAG_A);
                                    b->is_button = (c->tag == NS_TAG_BUTTON);
                                    b->action_id = c->action_id;
                                    ns_strcpy(b->lines[0], c->first_child->text, 128);
                                    b->line_count = 1;
                                }
                                int mr = c->style.margin_right > 0 ? c->style.margin_right : (c->next_sibling ? 6 : 0);
                                curr_x += text_w + mr;
                            } else {
                                ns_build_boxes(c, curr_x, &cur_line_y, inner_w - (curr_x - inner_x));
                            }
                        }
                        c = c->next_sibling;
                    }

                    cur_line_y += line_h;
                    line_start = line_end ? (line_end->tag == NS_TAG_BR ? line_end->next_sibling : line_end) : NULL;
                }
                inner_y = cur_line_y;
            } else {
                ns_node_t* child = node->first_child;
                while (child) {
                    ns_build_boxes(child, inner_x, &inner_y, inner_w);
                    child = child->next_sibling;
                }
            }
        }

        int content_h = inner_y - start_inner_y;
        if (node->style.height > 0) {
            box->height = node->style.height;
        } else {
            box->height = content_h + node->style.padding_top + node->style.padding_bottom;
            if (node->tag == NS_TAG_BUTTON && box->height < 28) box->height = 28;
            if (node->tag == NS_TAG_HR) box->height = 1;
        }

        *cur_y = box->y + box->height + node->style.margin_bottom;
    }
}

void netsurf_layout(int width) {
    num_boxes = 0;
    total_page_height = 0;
    if (!dom_root) return;

    int cur_y = 16;
    int margin_x = 24;
    int usable_w = width - (margin_x * 2);

    ns_node_t* child = dom_root->first_child;
    while (child) {
        ns_build_boxes(child, margin_x, &cur_y, usable_w);
        child = child->next_sibling;
    }

    total_page_height = cur_y + 32;

    serial_print("[NetSurf] Layout complete. Boxes: ");
    serial_print_dec(num_boxes);
    serial_print(", Total H: ");
    serial_print_dec(total_page_height);
    serial_println("px");
}

// NetSurf Framebuffer Plotter (NetSurf Redraw Engine)
void netsurf_render(int view_x, int view_y, int view_w, int view_h, int scroll_y) {
    gfx_set_clip(view_x, view_y, view_w, view_h);

    for (int i = 0; i < num_boxes; i++) {
        ns_box_t* b = &layout_boxes[i];
        int draw_x = view_x + b->x;
        int draw_y = view_y + b->y - scroll_y;

        // Culling: check if outside view rect
        if (draw_y + b->height < view_y || draw_y > view_y + view_h) continue;

        // 1 & 2. Draw Background & Border clipped to view_h
        int clip_box_y = draw_y;
        int clip_box_h = b->height;
        if (clip_box_y < view_y) {
            int diff = view_y - clip_box_y;
            clip_box_h -= diff;
            clip_box_y = view_y;
        }
        if (clip_box_y + clip_box_h > view_y + view_h) {
            clip_box_h = (view_y + view_h) - clip_box_y;
        }

        if (clip_box_h > 0) {
            if ((b->style.bg_color >> 24) != 0) {
                if (b->style.border_radius > 0) {
                    gfx_fill_rounded_rect(draw_x, clip_box_y, b->width, clip_box_h, b->style.border_radius, b->style.bg_color);
                } else {
                    gfx_fill_rect(draw_x, clip_box_y, b->width, clip_box_h, b->style.bg_color);
                }
            }

            if (b->style.border_width > 0 && (b->style.border_color >> 24) != 0) {
                if (b->style.border_radius > 0) {
                    gfx_draw_rounded_rect(draw_x, clip_box_y, b->width, clip_box_h, b->style.border_radius, b->style.border_color);
                } else {
                    gfx_draw_rect(draw_x, clip_box_y, b->width, clip_box_h, b->style.border_color);
                }
            }
        }

        // 3. Draw Text Lines (only visible rows)
        if (b->line_count > 0) {
            int ty = draw_y;
            int scale = (b->style.font_size >= 24) ? 2 : 1;
            int char_w = FONT_WIDTH * scale;
            for (int l = 0; l < b->line_count; l++) {
                if (ty >= view_y && (ty + (scale == 2 ? 30 : 14)) <= (view_y + view_h)) {
                    uint32_t col = b->is_link ? COLOR_BLUE_ACCENT : b->style.color;
                    int tx = draw_x;
                    int lw = ns_strlen(b->lines[l]) * char_w;
                    if (b->is_button && b->width > lw) {
                        tx = draw_x + (b->width - lw) / 2;
                    } else if (b->style.text_align == 1 && b->width > lw) {
                        tx = draw_x + (b->width - lw) / 2;
                    } else if (b->style.text_align == 2 && b->width > lw) {
                        tx = draw_x + (b->width - lw);
                    }
                    int max_text_w = b->width > 0 ? b->width : (view_w - (tx - view_x));
                    if (scale > 1) {
                        gfx_draw_text_scaled(tx, ty, b->lines[l], col, scale);
                    } else {
                        gfx_draw_text_clipped(tx, ty, max_text_w, b->lines[l], col);
                    }
                    if (b->is_link) {
                        int link_w = lw;
                        if (max_text_w > 0 && link_w > max_text_w) link_w = max_text_w;
                        gfx_fill_rect(tx, ty + (scale == 2 ? 26 : 12), link_w, 1, COLOR_BLUE_ACCENT);
                    }
                }
                ty += (scale == 2 ? 34 : (b->style.font_size + 2));
            }
        }
    }

    gfx_reset_clip();
}

const char* netsurf_get_title(void) {
    return page_title;
}

int netsurf_get_page_height(void) {
    return total_page_height;
}

const char* netsurf_handle_click(int rel_x, int rel_y, int scroll_y, int* out_action_id) {
    if (out_action_id) *out_action_id = 0;
    int page_y = rel_y + scroll_y;

    for (int i = 0; i < num_boxes; i++) {
        ns_box_t* b = &layout_boxes[i];
        if (rel_x >= b->x && rel_x < b->x + b->width &&
            page_y >= b->y && page_y < b->y + b->height) {

            // Check if link
            if (b->is_link) {
                if (b->node && b->node->href[0]) return b->node->href;
                if (b->node && b->node->parent && b->node->parent->href[0]) return b->node->parent->href;
            }
            // Check if button
            if (b->is_button) {
                if (out_action_id) *out_action_id = b->action_id ? b->action_id : (i + 1);
                return NULL;
            }
        }
    }
    return NULL;
}
