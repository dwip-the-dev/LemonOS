#ifndef ALERTS_H
#define ALERTS_H

#include <stdint.h>
#include <stddef.h>

#define MAX_ACTIVE_ALERTS 5
#define MAX_ALERT_HISTORY 20
#define ALERT_WIDTH       320
#define ALERT_HEIGHT      68

typedef enum {
    ALERT_TYPE_INFO = 0,
    ALERT_TYPE_SUCCESS = 1,
    ALERT_TYPE_WARNING = 2,
    ALERT_TYPE_ERROR = 3
} alert_type_t;

typedef struct {
    int          id;
    char         title[48];
    char         message[96];
    int          icon_id;
    alert_type_t type;
    int          duration_frames;
    int          remaining_frames;
    int          is_active;
    int          anim_x;
    int          target_x;
    uint32_t     timestamp_sec;
} alert_t;

void     alert_init(void);
int      alert_send(const char* title, const char* message, int icon_id, alert_type_t type);
void     alert_update(void);
void     alert_render(void);
int      alert_handle_mouse(int mx, int my, int clicked);

// Notification Center / History API
int      alert_get_history_count(void);
alert_t* alert_get_history(int index);
void     alert_clear_history(void);
int      alert_get_unread_count(void);
void     alert_mark_all_read(void);

#endif // ALERTS_H
