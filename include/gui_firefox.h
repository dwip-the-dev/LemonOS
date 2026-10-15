#ifndef GUI_FIREFOX_H
#define GUI_FIREFOX_H

#include "wm.h"

// Launch Firefox Web Browser window
void gui_firefox_open(void);

// Launch Firefox and immediately navigate to a specific URL
void gui_firefox_open_url(const char* url);

#endif
