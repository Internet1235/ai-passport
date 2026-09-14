idf_component_register(
    SRCS "starbridge_app.c" "starbridge.c" "starbridge_sound.c" "starbridge_audio.c"
         "../assets/fonts/starbridge_16.c" "../assets/fonts/starbridge_12.c"
    INCLUDE_DIRS "."
    REQUIRES bsp esp_timer nvs_flash esp_driver_usb_serial_jtag
)
