#ifndef NETSURF_H
#define NETSURF_H

#include <stdint.h>
#include <stddef.h>

#define NS_MAX_NODES     1024
#define NS_MAX_BOXES     1024
#define NS_MAX_RULES     128
#define NS_MAX_TEXT_LEN  1024
#define NS_MAX_URL_LEN   512

// DOM Node Types
typedef enum {
    NS_NODE_DOCUMENT,
    NS_NODE_ELEMENT,
    NS_NODE_TEXT,
    NS_NODE_COMMENT
} ns_node_type_t;

// HTML Tag Enumeration
typedef enum {
    NS_TAG_UNKNOWN = 0,
    NS_TAG_HTML,
    NS_TAG_HEAD,
    NS_TAG_TITLE,
    NS_TAG_STYLE,
    NS_TAG_BODY,
    NS_TAG_DIV,
    NS_TAG_SECTION,
    NS_TAG_HEADER,
    NS_TAG_FOOTER,
    NS_TAG_P,
    NS_TAG_H1,
    NS_TAG_H2,
    NS_TAG_H3,
    NS_TAG_H4,
    NS_TAG_H5,
    NS_TAG_H6,
    NS_TAG_A,
    NS_TAG_SPAN,
    NS_TAG_B,
    NS_TAG_STRONG,
    NS_TAG_EM,
    NS_TAG_I,
    NS_TAG_PRE,
    NS_TAG_CODE,
    NS_TAG_UL,
    NS_TAG_OL,
    NS_TAG_LI,
    NS_TAG_TABLE,
    NS_TAG_TR,
    NS_TAG_TH,
    NS_TAG_TD,
    NS_TAG_BUTTON,
    NS_TAG_INPUT,
    NS_TAG_HR,
    NS_TAG_BR,
    NS_TAG_IMG
} ns_tag_t;

// Computed CSS Style
typedef struct {
    uint32_t color;
    uint32_t bg_color;
    uint32_t border_color;
    uint8_t  border_width;
    uint8_t  border_radius;
    uint8_t  font_size;
    uint8_t  font_bold;
    uint8_t  text_align; // 0=left, 1=center, 2=right
    uint8_t  display;    // 0=inline, 1=block, 2=inline-block, 3=table, 4=table-row, 5=table-cell, 6=none
    int16_t  margin_top, margin_bottom, margin_left, margin_right;
    int16_t  padding_top, padding_bottom, padding_left, padding_right;
    int16_t  width, height;
} ns_style_t;

// DOM Tree Node
typedef struct ns_node {
    int id;
    ns_node_type_t type;
    ns_tag_t tag;
    char tag_name[16];
    char elem_id[24];
    char class_name[32];
    char href[NS_MAX_URL_LEN];
    char src[NS_MAX_URL_LEN];
    char text[NS_MAX_TEXT_LEN];
    int action_id;

    struct ns_node* parent;
    struct ns_node* first_child;
    struct ns_node* last_child;
    struct ns_node* next_sibling;
    struct ns_node* prev_sibling;

    ns_style_t style;
} ns_node_t;

// Render Layout Box (NetSurf Box Tree)
typedef struct {
    int x, y, width, height;
    int content_x, content_y, content_w, content_h;
    ns_node_t* node;
    ns_style_t style;

    char lines[32][128];
    int line_count;

    uint8_t is_link;
    uint8_t is_button;
    uint8_t is_input;
    int action_id;
} ns_box_t;

// NetSurf Engine Public API
void        netsurf_init(void);
void        netsurf_parse_html(const char* html, int len);
void        netsurf_layout(int width);
void        netsurf_render(int view_x, int view_y, int view_w, int view_h, int scroll_y);
const char* netsurf_get_title(void);
const char* netsurf_handle_click(int rel_x, int rel_y, int scroll_y, int* out_action_id);
int         netsurf_get_page_height(void);

#endif
