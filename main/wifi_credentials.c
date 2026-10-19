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
        return "请选择或输入 Wi-Fi 名称（SSID）。";
    }
    if (bounded_length(ssid, WIFI_SSID_MAX_BYTES + 1) > WIFI_SSID_MAX_BYTES) {
        return "Wi-Fi 名称不能超过 32 个 UTF-8 字节。";
    }
    if (password == NULL) {
        return "请输入密码；开放网络可以留空。";
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
                return "64 字节密码必须是十六进制 PSK。";
            }
        }
        return NULL;
    }
    return "密码须为 8–63 字节或 64 位十六进制 PSK；\n开放网络可留空。";
}
