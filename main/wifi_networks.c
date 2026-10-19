#include <string.h>
#include "wifi_networks.h"

void wifi_networks_add(wifi_network_t *networks, size_t *count,
                       const wifi_network_t *candidate)
{
    if (networks == NULL || count == NULL || candidate == NULL ||
        *count > WIFI_NETWORK_LIMIT || candidate->ssid[0] == '\0' ||
        memchr(candidate->ssid, '\0', sizeof(candidate->ssid)) == NULL) return;

    size_t index;
    for (index = 0; index < *count; ++index) {
        if (strcmp(networks[index].ssid, candidate->ssid) == 0 &&
            networks[index].password_required == candidate->password_required &&
            networks[index].supported == candidate->supported) {
            if (networks[index].rssi >= candidate->rssi) return;
            break;
        }
    }
    if (index == *count) {
        if (*count < WIFI_NETWORK_LIMIT) ++*count;
        else {
            index = *count - 1;
            if (networks[index].rssi >= candidate->rssi) return;
        }
    }
    networks[index] = *candidate;
    /* A new/stronger entry can only move toward the front. */
    while (index > 0 && networks[index].rssi > networks[index - 1].rssi) {
        wifi_network_t previous = networks[index - 1];
        networks[index - 1] = networks[index];
        networks[index] = previous;
        --index;
    }
}
