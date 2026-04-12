#ifndef WIFI_STATE_H
#define WIFI_STATE_H

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CONNECT_STATUS_IDLE = 0,
    CONNECT_STATUS_CONNECTING,
    CONNECT_STATUS_CONNECTED,
    CONNECT_STATUS_FAILED
} wifi_connect_status_t;

extern volatile wifi_connect_status_t s_connect_status;
extern char s_connect_fail_reason[64];
extern char s_sta_ip_addr[16];
extern EventGroupHandle_t s_wifi_event_group;

static const int CONNECTED_BIT = BIT0;
static const int FAILED_BIT = BIT1;

#ifdef __cplusplus
}
#endif

#endif
