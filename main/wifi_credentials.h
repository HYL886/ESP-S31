#pragma once

#define WIFI_SSID_MAX_BYTES 32
#define WIFI_PASSWORD_MAX_BYTES 64

typedef struct {
    char ssid[WIFI_SSID_MAX_BYTES + 1];
    char password[WIFI_PASSWORD_MAX_BYTES + 1];
} wifi_credentials_t;

/* Returns NULL for valid input, or an explanation suitable for the screen. */
const char *wifi_credentials_error(const char *ssid, const char *password);
