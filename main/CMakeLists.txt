file(GLOB HELIX_SOURCES "vendor/helix/*.c")
idf_component_register(
    SRCS ${HELIX_SOURCES} "walkman_app.c" "walkman.c" "walkman_audio.c" "walkman_decoder.c" "walkman_tracks.c"
         "../assets/fonts/walkman_14.c" "../assets/fonts/walkman_16.c" "../assets/fonts/walkman_12.c"
    INCLUDE_DIRS "."
    REQUIRES bsp esp_timer nvs_flash esp_driver_usb_serial_jtag
    EMBED_FILES "../assets/music/walkman/audio.bin"
)

set_source_files_properties(${HELIX_SOURCES} PROPERTIES COMPILE_OPTIONS "-O2;-fwrapv;-Wno-error")
