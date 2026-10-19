/* SPDX-License-Identifier: CC0-1.0 */
#include <stddef.h>
#include "wifi_credentials.h"

static size_t bounded_length(const char *text, size_t limit)
{
    size_t length = 0;
    while (length < limit && text[length] != '\0') {
        ++length;
    }
    return length;
}

const char *wifi_credentials_error(const char *ssid, const char *password)
{
    if (ssid == NULL || ssid[0] == '\0') {
        return "Enter the Wi-Fi name (SSID).";
    }
    if (bounded_length(ssid, WIFI_SSID_MAX_BYTES + 1) > WIFI_SSID_MAX_BYTES) {
        return "Wi-Fi name must fit in 32 UTF-8 bytes.";
    }
    if (password == NULL) {
        return "Enter a password, or leave it empty for an open network.";
    }
    size_t length = bounded_length(password, WIFI_PASSWORD_MAX_BYTES + 1);
    if (length == 0 || (length >= 8 && length <= 63)) {
        return NULL;
    }
    if (length == 64) {
        for (size_t i = 0; i < length; ++i) {
            char c = password[i];
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
                  (c >= 'A' && c <= 'F'))) {
                return "A 64-byte password must be a hexadecimal PSK.";
            }
        }
        return NULL;
    }
    return "Password: 8-63 bytes, 64 hex digits, or empty for open Wi-Fi.";
}
