#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WIFI_MANAGER_EVENT_CONNECTED = 0,
    WIFI_MANAGER_EVENT_DISCONNECTED,
    WIFI_MANAGER_EVENT_FAIL
} wifi_manager_event_t;

typedef void (*wifi_event_cb_t)(wifi_manager_event_t event);

void wifi_manager_register_callback(wifi_event_cb_t cb);
esp_err_t wifi_manager_start(void);
void wifi_manager_stop(void);

#ifdef __cplusplus
}
#endif
