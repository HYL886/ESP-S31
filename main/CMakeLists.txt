idf_component_register(
    SRCS "main.c" "wifi_clock.c" "wifi_setup.c" "wifi_credentials.c" "wifi_networks.c"
         "ui_fonts.c" "fonts/s31_cn_16.c" "fonts/s31_cn_24.c" "fonts/s31_cn_32.c"
         "camera_preview.c" "camera_frame.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_wifi esp_event esp_netif esp_timer nvs_flash
)
