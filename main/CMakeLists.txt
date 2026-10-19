idf_component_register(
    SRCS "main.c" "wifi_clock.c" "wifi_setup.c" "wifi_credentials.c" "wifi_networks.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_wifi esp_event esp_netif esp_timer nvs_flash
)
