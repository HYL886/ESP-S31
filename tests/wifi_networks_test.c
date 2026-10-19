#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "wifi_networks.h"

static wifi_network_t entry(const char *ssid, int8_t rssi, bool secured)
{
    wifi_network_t result = {.rssi = rssi, .password_required = secured, .supported = true};
    snprintf(result.ssid, sizeof(result.ssid), "%s", ssid);
    return result;
}

int main(void)
{
    wifi_network_t networks[WIFI_NETWORK_LIMIT] = {0};
    size_t count = 0;
    wifi_network_t ap = entry("Home", -70, true);
    wifi_networks_add(networks, &count, &ap);
    ap.rssi = -40;
    wifi_networks_add(networks, &count, &ap);
    assert(count == 1 && networks[0].rssi == -40);
    ap.rssi = -90;
    wifi_networks_add(networks, &count, &ap);
    assert(count == 1 && networks[0].rssi == -40);

    ap = entry("Home", -30, false); /* Same name, different security: separate row. */
    wifi_networks_add(networks, &count, &ap);
    assert(count == 2 && !networks[0].password_required);
    ap = entry("\xe5\xae\xb6\xe5\xba\xadWiFi", -20, true);
    wifi_networks_add(networks, &count, &ap);
    assert(strcmp(networks[0].ssid, ap.ssid) == 0);
    ap = entry("12345678901234567890123456789012", -25, true);
    wifi_networks_add(networks, &count, &ap);
    assert(strlen(networks[1].ssid) == 32);

    size_t previous = count;
    ap = entry("", -10, true);
    wifi_networks_add(networks, &count, &ap);
    memset(ap.ssid, 'x', sizeof(ap.ssid));
    wifi_networks_add(networks, &count, &ap);
    wifi_networks_add(networks, &count, NULL);
    assert(count == previous);

    for (int i = 0; i < 30; ++i) {
        ap = entry("", (int8_t)(-50 - i), true);
        snprintf(ap.ssid, sizeof(ap.ssid), "Network-%d", i);
        wifi_networks_add(networks, &count, &ap);
    }
    assert(count == WIFI_NETWORK_LIMIT);
    for (size_t i = 1; i < count; ++i) assert(networks[i - 1].rssi >= networks[i].rssi);
    ap = entry("Too weak", -110, true);
    wifi_networks_add(networks, &count, &ap);
    for (size_t i = 0; i < count; ++i) assert(strcmp(networks[i].ssid, "Too weak") != 0);
    ap = entry("New strongest", -10, true);
    wifi_networks_add(networks, &count, &ap);
    assert(count == WIFI_NETWORK_LIMIT && strcmp(networks[0].ssid, "New strongest") == 0);
    ap = entry("Network-10", -5, true); /* Improve an existing row in a full list. */
    wifi_networks_add(networks, &count, &ap);
    assert(count == WIFI_NETWORK_LIMIT && strcmp(networks[0].ssid, "Network-10") == 0);

    puts("Wi-Fi network ordering/deduplication tests passed.");
    return 0;
}
