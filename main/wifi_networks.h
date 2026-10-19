#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "wifi_credentials.h"

#define WIFI_NETWORK_LIMIT 20

typedef struct {
    char ssid[WIFI_SSID_MAX_BYTES + 1];
    int8_t rssi;
    bool password_required;
    bool supported;
} wifi_network_t;

/* Keep the strongest entry for each name/security type, in RSSI order. */
void wifi_networks_add(wifi_network_t *networks, size_t *count,
                       const wifi_network_t *candidate);
