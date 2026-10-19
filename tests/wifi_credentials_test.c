#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "wifi_credentials.h"

int main(void)
{
    char ssid[34];
    char password[66];
    assert(wifi_credentials_error(NULL, "12345678") != NULL);
    assert(wifi_credentials_error("", "12345678") != NULL);
    assert(wifi_credentials_error("Home", NULL) != NULL);
    assert(wifi_credentials_error("Open network", "") == NULL);
    assert(wifi_credentials_error("Home", "1234567") != NULL);
    assert(wifi_credentials_error("Home", "12345678") == NULL);

    memset(ssid, 's', 32);
    ssid[32] = '\0';
    assert(wifi_credentials_error(ssid, "12345678") == NULL);
    ssid[32] = 's';
    ssid[33] = '\0';
    assert(wifi_credentials_error(ssid, "12345678") != NULL);

    /* Eleven 3-byte UTF-8 characters exceed the SSID limit of 32 bytes. */
    assert(wifi_credentials_error(
        "\xe4\xb8\xad\xe4\xb8\xad\xe4\xb8\xad\xe4\xb8\xad\xe4\xb8\xad"
        "\xe4\xb8\xad\xe4\xb8\xad\xe4\xb8\xad\xe4\xb8\xad\xe4\xb8\xad"
        "\xe4\xb8\xad", "12345678") != NULL);

    memset(password, 'p', 63);
    password[63] = '\0';
    assert(wifi_credentials_error("Home", password) == NULL);
    password[63] = 'p';
    password[64] = '\0';
    assert(wifi_credentials_error("Home", password) != NULL);
    memset(password, 'A', 64);
    assert(wifi_credentials_error("Home", password) == NULL);
    password[64] = 'A';
    password[65] = '\0';
    assert(wifi_credentials_error("Home", password) != NULL);
    assert(wifi_credentials_error("  Home  ", "with spaces!") == NULL);
    puts("Wi-Fi credential boundary tests passed.");
    return 0;
}
