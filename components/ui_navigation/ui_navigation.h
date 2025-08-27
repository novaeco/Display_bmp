#ifndef UI_NAVIGATION_H
#define UI_NAVIGATION_H

#include <stdint.h>

#define TEXT_X_DIVISOR           5
#define TEXT_Y1_DIVISOR          3
#define TEXT_LINE_SPACING        40
#define BTN_WIDTH                320
#define BTN_HEIGHT               120
#define NAV_MARGIN               20
#define ARROW_WIDTH              60
#define ARROW_HEIGHT             60
#define BTN_LABEL_L_OFFSET_X     60
#define BTN_LABEL_R_OFFSET_X     40
#define BTN_LABEL_N_OFFSET_X     20
#define BTN_LABEL_OFFSET_Y       12
#define HOME_TOUCH_WIDTH         NAV_MARGIN
#define HOME_TOUCH_HEIGHT        ARROW_HEIGHT
#define FILENAME_BAR_PAD         2

typedef enum {
    NAV_NONE = 0,
    NAV_EXIT,
    NAV_HOME,
    NAV_ZOOM_IN,
    NAV_ZOOM_OUT,
    NAV_SCROLL,
    NAV_ROTATE
} nav_action_t;

typedef enum {
    NAV_CMD_PREV  = -1,
    NAV_CMD_NONE  = 0,
    NAV_CMD_NEXT  = 1,
    NAV_CMD_ROTATE = 2,
    NAV_CMD_HOME   = 3,
    NAV_CMD_EXIT   = 4
} nav_cmd_t;

typedef enum {
    IMAGE_SOURCE_LOCAL = 0,
    IMAGE_SOURCE_REMOTE,
    IMAGE_SOURCE_NETWORK
} image_source_t;

const char *draw_folder_selection(void);
void draw_navigation_arrows(void);
void draw_filename_bar(const char *path);
void ui_navigation_show_image(const char *path);
nav_action_t handle_touch_navigation(int8_t *idx);
image_source_t draw_source_selection(void);
void ui_navigation_deinit(void);

#endif // UI_NAVIGATION_H
