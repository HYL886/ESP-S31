set(S31_BACKGROUND_FILE "${CMAKE_CURRENT_SOURCE_DIR}/assets/home_background.rgb565")
set(S31_BACKGROUND_ASM "${CMAKE_CURRENT_BINARY_DIR}/background_asset.S")
if(NOT CMAKE_BUILD_EARLY_EXPANSION)
    configure_file("background_asset.S.in" "${S31_BACKGROUND_ASM}" @ONLY)
    set_source_files_properties("${S31_BACKGROUND_ASM}" PROPERTIES
        OBJECT_DEPENDS "${S31_BACKGROUND_FILE}")
endif()

idf_component_register(
    SRCS "main.c" "wifi_clock.c" "wifi_setup.c" "wifi_credentials.c" "wifi_networks.c"
         "ui_fonts.c" "fonts/s31_cn_16.c" "fonts/s31_cn_24.c" "fonts/s31_cn_32.c"
         "camera_preview.c" "camera_frame.c" "ui_background.c"
         "${S31_BACKGROUND_ASM}"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_wifi esp_event esp_netif esp_timer nvs_flash
)
